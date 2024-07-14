#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace HPGS{

static HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");

//thread_local 便是在每个线程都会有自己该属性
static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fibre* t_scheduler_fibre = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : m_name(name){
    HPGS_ASSERT(threads > 0);

    if(use_caller) {
        HPGS::Fibre::GetThis();
        --threads;

        HPGS_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

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

void Scheduler::stop(){
    m_autoStop = true;
    if(m_rootFibre && m_threadCount == 0 && 
            (m_rootFibre->getState() == Fibre::TERM || m_rootFibre->getState() == Fibre::INIT)){
        HPGS_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if(stopping()){
            return;
        }    
    }

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
            m_rootFibre->call();
        }
    }

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

void Scheduler::run(){
    HPGS_LOG_DEBUG(g_logger) << m_name << " run";
    set_hook_enable(true);
    setThis();
    if(HPGS::GetThreadId() != m_rootThread){
        t_scheduler_fibre = Fibre::GetThis().get();
    }

    Fibre::ptr idle_fibre(new Fibre(std::bind(&Scheduler::idle, this)));
    Fibre::ptr cb_fibre;

    FibreAndThread ft;
    while(true){
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_fibres.begin();
            while(it != m_fibres.end()){
                if(it->thread != -1 && it->thread != HPGS::GetThreadId()){
                    it++;
                    tickle_me = true;
                    continue;
                }

                HPGS_ASSERT(it->fibre || it->cb);
                if(it->fibre && it->fibre->getState() == Fibre::EXEC){
                    it++;
                    continue;
                }

                ft = *it;
                m_fibres.erase(it++);
                m_activeThreadCount++;
                is_active = true;
                break;
            }
            tickle_me |= it != m_fibres.end();
        }

        if(tickle_me){
            tickle();
        }

        if(ft.fibre && (ft.fibre->getState() != Fibre::TERM 
                && ft.fibre->getState() != Fibre::EXCEPT)){
            ft.fibre->swapIn();
            m_activeThreadCount--;

            if(ft.fibre->getState() == Fibre::READY){
                schedule(ft.fibre);
            }
            else if(ft.fibre->getState() != Fibre::TERM && ft.fibre->getState() != Fibre::EXCEPT){
                ft.fibre->setState(Fibre::HOLD);
            }
            ft.reset();
        }
        else if(ft.cb){
            if(cb_fibre){
                cb_fibre->reset(ft.cb);
            }
            else{
                cb_fibre.reset(new Fibre(ft.cb));
            }

            ft.reset();
            cb_fibre->swapIn();
            m_activeThreadCount--;

            if(cb_fibre->getState() == Fibre::READY){
                schedule(cb_fibre);
                cb_fibre.reset();
            }
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

            m_idleThreadCount++;
            idle_fibre->swapIn();
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
