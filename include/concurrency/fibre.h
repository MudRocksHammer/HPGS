#ifndef __HPGS_FIBRE_H__
#define __HPGS_FIBRE_H__


#include <memory>
#include <functional>
#include <ucontext.h>

namespace HPGS{

class Scheduler;

class Fibre : public std::enable_shared_from_this<Fibre> {
public:
    typedef std::shared_ptr<Fibre> ptr;

    enum State {
        INIT,       //初始化状态
        HOLD,       //暂停状态
        EXEC,       //执行状态
        TERM,       //结束状态
        READY,      //可执行状态
        EXCEPT      //异常状态
    };

private:
    /**
     * @brief 无参构造函数
     * @attention 每个线程的第一个协程，每个线程有一个主协程 
     */
    Fibre();

public:
    /**
     * @brief 构造函数
     * @param[in] cb 协程的执行函数
     * @param[in] stacksize 协程栈大小
     * @param[in] use_caller 是否在MainFibre上
     */
    Fibre(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);

    ~Fibre();

    /**
     * @brief 重置协程执行函数，并设置状态
     * @pre getState() 为 INIT, TERM, EXCEPT
     * @post getState() = INIT
     */
    void reset(std::function<void()> cb);

    /**
     * @brief 将当前协程切换到运行状态
     * @pre getState() != EXEC
     * @post getState() = EXEC
     */
    void swapIn();

    /**
     * @brief 切换当前协程到后台
     */
    void swapOut();

    /**
     * @brief 将当前协程切换到执行态
     * @pre 执行的为当前的主协程
     */
    void call();

    /**
     * @brief 将当前协程切换到后台
     * @pre 执行的为该协程
     * @post 返回到线程的主协程
     */
    void back();
    
    uint64_t getId() const { return m_id; }

    State getState() const { return m_state;}

public:
    /**
     * @brief 设置当前线程运行的协程
     * @param[in] f 运行协程
     */
    static void SetThis(Fibre* f);

    /**
     * @brief 获取当前执行的协程
     */
    static Fibre::ptr GetThis();

        /**
     * @brief 创建主协程
     */
    static void createMainFibre();

    /**
     * @brief 当前协程放弃cpu切换到后台并设置为READY状态
     * @post getState() = READY
     */
    static void YieldToReady();

    /**
     * @brief 切换到后台并设置为HOLD状态
     * @post getState() = HOLD
     */
    static void YieldToHold();

    static uint64_t TotalFibres();

    /**
     * @brief 协程执行函数
     * @post 执行完返回到线程的主协程
     */
    static void MainFunc();

    /**
     * @brief 主协程执行函数
     * @post 执行完返回到线程调度协程
     */
    static void CallerMainFunc();

    /**
     * @brief 获取当前协程的Id
     */
    static uint64_t GetFibreId();

    void setState(State state){ m_state = state; }

private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = INIT;
    ucontext_t m_ctx;
    void* m_stack = nullptr;        //协程拥有的栈空间指针
    std::function<void()> m_cb;
};

}

#endif