/**
 * @file scheduler.h
 * @brief 协程调度器封装
 */
#ifndef __HPGS_SCHEDULER_H__
#define __HPGS_SCHEDULER_H__


#include <memory>
#include <vector>
#include <list>
#include <iostream>
#include "fibre.h"
#include "thread.h"


namespace HPGS{

/**
 * @brief 协程调度器
 * @details 封装的是N-M的协程调度器，内部有一个线程池，支持线程在线程池里切换
 */
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    /**
     * @brief 构造函数
     * @param[in] threads 线程数量
     * @param[in] use_caller 是否使用当前调用线程
     * @param[in] name 协程调度器名称
     */
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    virtual ~Scheduler();

    const std::string& getName() const { return m_name; }

    static Scheduler* GetThis();

    static Fibre* GetMainFibre();

    void start();

    void stop();

    /**
     * @brief 协程调度
     * @param[in] fc 协程或函数
     * @param[in] thread 协程执行的线程id，-1表示任意线程
     */
    template<class FibreOrCb>
    void schedule(FibreOrCb fc, int thread = -1){
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }

        if(need_tickle){
            tickle();
        }
    }

    /**
     * @brief 批量协程调度
     * @param[in] begin 协程数组的开始
     * @param[in] end 协程数组的结束
     */
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end){
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end){
                need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                begin++;
            }
        }
        if(need_tickle){
            tickle();
        }
    }

    void switchTo(int thread = -1);
    std::ostream dump(std::ostream& os);

protected:
    /**
     * @brief 通知调度器有任务
     */
    virtual void tickle();

    /**
     * @brief 协程调度函数
     */
    void run();

    /**
     * @brief 返回是否可以停止
     */
    virtual bool stopping();

    /**
     * @brief 协程无任务可调度时执行idle函数
     */
    virtual void idle();

    void setThis();

    /**
     * @brief 是否有空闲线程
     */
    bool hasIdleThreads() { return m_idleThreadCount > 0; }

private:
    /**
     * @brief 协程调度启动
     */
    template<class FibreOrCb>
    bool scheduleNoLock(FibreOrCb fc, int thread){
        bool need_tickle = m_fibres.empty();
        FibreAndThread ft(fc, thread);
        if(ft.fibre || ft.cb){
            m_fibres.push_back(ft);
        }
        return need_tickle;
    }
    
    /**
     * @brief 协程/函数/线程组
     */
    struct FibreAndThread{
        Fibre::ptr fibre;
        std::function<void()> cb;
        int thread;

        /**
         * @brief 构造函数
         */
        FibreAndThread(Fibre::ptr f, int thr) : fibre(f), thread(thr){

        }

        FibreAndThread(Fibre::ptr* f, int thr) : thread(thr){
            fibre.swap(*f);
        }

        FibreAndThread(std::function<void()> f, int thr) : cb(f), thread(thr){

        }

        FibreAndThread(std::function<void()>* f, int thr) : thread(thr){
            cb.swap(*f);
        }

        FibreAndThread() : thread(-1){

        }

        void reset() {
            fibre = nullptr;
            cb = nullptr;
            thread = -1;
        }
        
    };

private:
    MutexType m_mutex;
    //线程池
    std::vector<Thread::ptr> m_threads;
    //执行的协程队列
    std::list<FibreAndThread> m_fibre;
    //use_caller 为true时有效，调度协程
    Fibre::ptr m_rootFibre;
    //协程调度器名称
    std::string m_name;

protected:
    //线程写的线程id数组
    std::vector<int> m_threadIds;
    //线程数量
    size_t m_threadCoumt = 0;
    //工作线程数量
    std::atomic<size_t> m_activeThreadCount = {0};
    //空闲线程数量
    std::atomic<size_t> m_idleThreadCount = {0};
    //是否正在停止
    bool m_stopping = true;
    //是否自动停止
    bool m_autoStop = false;
    //主线程id(use_caller)
    int m_rootThread = 0;
};

class SchedulerSwitcher : public Noncopyable {
public:
    SchedulerSwitcher(Scheduler* target = nullptr);
    ~SchedulerSwitcher();

private:
    Scheduler* m_caller;
};

}

#endif