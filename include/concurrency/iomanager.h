
#ifndef __HPGS_IOMANAGER_H__
#define __HPGS_IOMANAGER_H__


#include "scheduler.h"
#include "timer.h"


namespace HPGS{

/**
 * @brief 基于epoll的IO协程调度器
 */
class IOManager : public Scheduler, public TimerManager{
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    /**
     * @brief IO事件
     */
    enum Event{
        NONE = 0x0,     //无事件
        READ = 0x1,     //读事件(EPOLLIN)
        WRITE = 0x4     //写事件(EPOLLOUT)
    };

private:
    /**
     * @brief Socket事件上下文类 
     */
    struct FdContext {
        typedef Mutex MutexType;
        /**
         * @brief 事件上下文类
         */
        struct EventContext {
            //事件执行调度器
            Scheduler* scheduler = nullptr;
            //事件协程
            Fibre::ptr fibre;
            //事件回调函数
            std::function<void()> cb;
        };

        /**
         * @brief 获取事件上下文类
         * @param[in] event 事件类型
         * @return 返回对应事件上下文
         */
        EventContext& getContext(Event event);

        /**
         * @brief 重置事件上下文
         * @param[in, out] ctx 待重置上下文类
         */
        void resetContext(EventContext& ctx);

        /**
         * @brief 触发事件
         * @param[in] event 事件类型
         */
        void triggerEvent(Event event);

        //读事件上下文
        EventContext read; 
        //写事件上下文
        EventContext write;
        //事件关联fd
        int fd = 0;
        //当前事件
        Event events = NONE;
        //事件的mutex
        MutexType mutex;
    };

public:
    /**
     * @brief 构造函数
     * @param[in] thread 线程数量
     * @param[in] use_caller 是否将调用线程包含进去
     * @param[in] name 调度器名称
     */
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& = "");

    ~IOManager();

    /**
     * @brief 添加事件
     * @param[in] fd socket fd
     * @param[in] event 事件类型
     * @param[in] cb 事件回调函数
     * @return 添加成功返回0，失败返回-1 
     */
    int addEvent(int fd, Event evnet, std::function<void()> cb = nullptr);

    /**
     * @brief 删除事件
     * @param[in] fd socket fd
     * @param[in] event 事件类型
     * @attention 不会触发事件
     */
    bool delEvent(int fd, Event event);

    /**
     * @brief 取消事件
     * @param[in] fd socket fd
     * @param[in] event 事件类型
     * @attention 如果事件存在则触发事件
     */
    bool cancelEvent(int fd, Event event);

    /**
     * @brief 取消所有事件
     * @param[in] fd socket fd
     */
    bool cancelAll(int fd);

    /**
     * @brief 返回当前IOManager
     */
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertAtFront() override;

    /**
     * @brief 重置socket fd 上下文的容器大小
     * @param[in] size 容量大小
     */
    void contextResize(size_t size);

    /**
     * @brief 判断是否可以停止
     * @param[out] timeout 最近要出发的定时器事件间隔
     * @return 返回是否可以停止
     */
    bool stopping(uint64_t& timeout);

private:
    //epoll fd
    int m_epfd = 0;
    //pipe 文件 fd
    int m_tickleFds[2];
    //当前等待执行的事件数量
    std::atomic<size_t> m_pendingEventCount = {0};
    //IOManager mutex
    RWMutexType m_mutex;
    //socket 事件上下文容器
    std::vector<FdContext*> m_fdContexts; 
};

}

#endif