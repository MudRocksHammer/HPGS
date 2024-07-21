#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace HPGS{

static HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");

//thread_local 便是在每个线程都会有自己该属性
static thread_local Scheduler* t_scheduler = nullptr;
//调度器主协程原始指针
static thread_local Fibre* t_scheduler_fibre = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : m_name(name){
    HPGS_ASSERT(threads > 0);

    //把调度线程加入到caller线程中，即把caller线程也当作调度器的工作线程
    if(use_caller) {
        //创建协程
        HPGS::Fibre::GetThis();
        --threads;

        HPGS_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        //绑定成员函数时都需要绑定对象实例
        m_rootFibre.reset(new Fibre(std::bind(&Scheduler::run, this), 0, true));
        HPGS::Thread::SetName(m_name);
        
        t_scheduler_fibre = m_rootFibre.get();
        m_rootThread = HPGS::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    }
    else{
        m_rootThread = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler(){
    HPGS_ASSERT(m_stopping);
    if(GetThis() == this){
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis(){
    return t_scheduler;
}

Fibre* Scheduler::GetMainFibre(){
    return t_scheduler_fibre;
}

void Scheduler::start(){
    MutexType::Lock lock(m_mutex);
    //只能启动一次
    if(!m_stopping){
        return;
    }
    m_stopping = false;
    HPGS_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; i++){
        m_threads[i].reset(new Thread(
            std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    lock.unlock();
}

/**
 * stop分两种情况，一个是caller线程上也运行调度协程工作的情况，
 * 第二是调度器独占线程的情况，此时只需简单的等待各调度协程完成工作退出即可。
 */
void Scheduler::stop(){
    m_autoStop = true;
    //caller线程添加了调度协程，且调度线程数为0
    if(m_rootFibre && m_threadCount == 0 && 
            (m_rootFibre->getState() == Fibre::TERM || m_rootFibre->getState() == Fibre::INIT)){
        HPGS_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if(stopping()){
            return;
        }    
    }

    //
    if(m_rootThread != -1){
        HPGS_ASSERT(GetThis() == this);
    }
    else{
        HPGS_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; i++){
        tickle();
    }

    if(m_rootFibre){
        tickle();
    }

    
    if(m_rootFibre){
        if(!stopping()){
            //将caller线程的调度协程切换到工作态
            m_rootFibre->call();
        }
    }

    //空线程池换出来销毁
    std::vector<Thread::ptr> threads;
    {
        MutexType::Lock lock(m_mutex);
        threads.swap(m_threads);
    }

    for(auto& i : threads){
        i->join();
    }
}

void Scheduler::setThis(){
    t_scheduler = this;
}

/**
 * 调度函数负责在任务队列中取出任务执行，即调度子协程运行
 * 每个调度的子协程执行完后必须切换回调度协程
 * 在非caller线程里，调度协程就是调度线程的主协程，在caller线程里调度协程不是主协程
 * 
 */
void Scheduler::run(){
    HPGS_LOG_DEBUG(g_logger) << m_name << " run";
    set_hook_enable(true);
    setThis();
    if(HPGS::GetThreadId() != m_rootThread){
        t_scheduler_fibre = Fibre::GetThis().get();
    }

    Fibre::ptr idle_fibre(new Fibre(std::bind(&Scheduler::idle, this)));
    HPGS_LOG_INFO(g_logger) << "create idle fibre";
    Fibre::ptr cb_fibre;

    FibreAndThread ft;
    //线程执行调度任务
    while(true){
        ft.reset();
        //找到新的可执行任务
        bool tickle_me = false;
        //还有可执行任务
        bool is_active = false;
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_fibres.begin();
            //有任务时遍历
            while(it != m_fibres.end()){
                //任务有指定的线程执行
                if(it->thread != -1 && it->thread != HPGS::GetThreadId()){
                    it++;
                    tickle_me = true;
                    continue;
                }

                //thread == -1表示任意线程可执行，断言有任务可执行
                HPGS_ASSERT(it->fibre || it->cb);
                //遍历到的fibre正在执行，跳过
                if(it->fibre && it->fibre->getState() == Fibre::EXEC){
                    it++;
                    continue;
                }

                //找到一个可执行fibre，赋值给ft
                ft = *it;
                m_fibres.erase(it++);
                m_activeThreadCount++;
                is_active = true;
                break;
            }//end while
            tickle_me |= it != m_fibres.end();
        }

        if(tickle_me){
            tickle();
        }

        //如果任务是一个协程
        if(ft.fibre && (ft.fibre->getState() != Fibre::TERM 
                && ft.fibre->getState() != Fibre::EXCEPT)){
            //协程可执行，swapin
            ft.fibre->swapIn();

            //子协程执行完毕或被换出，yield到主协程，active--
            m_activeThreadCount--;

            //如果任务还没执行完毕，再次加入任务列表
            if(ft.fibre->getState() == Fibre::READY){
                schedule(ft.fibre);
            }
            else if(ft.fibre->getState() != Fibre::TERM && ft.fibre->getState() != Fibre::EXCEPT){
                ft.fibre->setState(Fibre::HOLD);
            }
            //重置ft，查找下一个需要执行的任务
            ft.reset();
        }
        //任务只是一个函数，为这个函数创建一个fibre
        else if(ft.cb){
            if(cb_fibre){
                cb_fibre->reset(ft.cb);
            }
            else{
                cb_fibre.reset(new Fibre(ft.cb));
            }

            ft.reset();
            //创建的fibre开始执行
            cb_fibre->swapIn();

            //执行完毕或被换出
            m_activeThreadCount--;

            //任务还未执行完毕
            if(cb_fibre->getState() == Fibre::READY){
                schedule(cb_fibre);
                //指针放弃对象
                cb_fibre.reset();
            }
            //任务执行完毕，重置fibre的函数
            else if(cb_fibre->getState() == Fibre::EXCEPT || cb_fibre->getState() == Fibre::TERM){
                cb_fibre->reset(nullptr);
            }
            else{
                cb_fibre->setState(Fibre::HOLD);
                cb_fibre.reset();
            }
        }//end else if(ft.cb)
        else{
            if(is_active){
                m_activeThreadCount--;
                continue;
            }
            if(idle_fibre->getState() == Fibre::TERM){
                HPGS_LOG_INFO(g_logger) << "idle fibre term";
                break;
            }

            //没有任务执行，切换到idle
            m_idleThreadCount++;
            idle_fibre->swapIn();

            //stopping = true 或 idle被换出来了
            m_idleThreadCount--;
            if(idle_fibre->getState() != Fibre::TERM && idle_fibre->getState() != Fibre::EXCEPT){
                idle_fibre->setState(Fibre::HOLD);
            }
        }//end else
    }//end while true
}

void Scheduler::tickle(){
    HPGS_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autoStop && m_stopping && m_fibres.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle(){
    HPGS_LOG_INFO(g_logger) << "idle";
    while(!stopping()){
        HPGS::Fibre::YieldToHold();
    }
}

void Scheduler::switchTo(int thread){
    HPGS_ASSERT(Scheduler::GetThis() != nullptr);
    if(Scheduler::GetThis() == this){
        if(thread == -1 || thread == HPGS::GetThreadId()){
            return;
        }
    }
    schedule(Fibre::GetThis(), thread);
    Fibre::YieldToHold();
}

std::ostream& Scheduler::dump(std::ostream& os){
    os << "[Scheduler name = " << m_name 
       << " size = " << m_threadCount
       << " active_count = " << m_activeThreadCount
       << " idle_count = " << m_idleThreadCount
       << "stopping = " << m_stopping
       << " ]" << std::endl << "   ";
    for(size_t i = 0; i < m_threadIds.size(); i++){
        if(i){
            os << ", ";
        }
        os << m_threadIds[i];
    }
    return os;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target){
    m_caller = Scheduler::GetThis();
    if(target){
        target->switchTo();
    }
}

SchedulerSwitcher::~SchedulerSwitcher(){
    if(m_caller){
        m_caller->switchTo();
    }
}

}
