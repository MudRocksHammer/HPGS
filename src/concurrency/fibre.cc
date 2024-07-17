#include "fibre.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"
#include <atomic>

#define USE_SCHEDULER 1

namespace HPGS{

static Logger::ptr g_logger = HPGS_LOG_NAME("system");

static std::atomic<uint64_t> s_fibre_id{0};
static std::atomic<uint64_t> s_fibre_count{0};

//线程当前执行的协程
static thread_local Fibre* t_fibre = nullptr;

//该线程的主协程,使用调度器的话这里没有用，主协程在调度器里
static thread_local Fibre::ptr t_threadFibre = nullptr;

static ConfigVar<uint32_t>::ptr g_fibre_stack_size = 
        Config::Lookup<uint32_t>("fibre.stack_size", 128 * 1024, "fibre stack size");

class MallocStackAllocator{
public:
    static void* Alloc(size_t size){
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t size){
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fibre::GetFibreId(){
    if(t_fibre){
        return t_fibre->getId();
    }
    return 0;
}

//默认协程构造函数做线程的任务，不需要分配协程栈
Fibre::Fibre(){
    m_state = EXEC;
    SetThis(this);

    //获取当前上下文(包括寄存器，堆栈指针等),如果获取失败则调用断言打印调用栈然后结束程序
    if(getcontext(&m_ctx)){
        HPGS_ASSERT2(false, "getcontext");
    }

    ++s_fibre_count;

    HPGS_LOG_DEBUG(g_logger) << "Fibre::fibre main";
}

Fibre::Fibre(std::function<void()> cb, size_t stacksize, bool use_caller)
: m_id(++s_fibre_id), m_cb(cb){
    ++s_fibre_count;
    m_stacksize = stacksize ? stacksize : g_fibre_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)){
        HPGS_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    if(!use_caller){
        makecontext(&m_ctx, &Fibre::MainFunc, 0);
    }
    else{
        makecontext(&m_ctx, &Fibre::CallerMainFunc, 0);
        //SetThis(this);
    }

    HPGS_LOG_DEBUG(g_logger) << "Fibre::Fibre id = " << m_id;
}

Fibre::~Fibre(){
    --s_fibre_count;
    if(m_stack){
        HPGS_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
        StackAllocator::Dealloc(m_stack, m_stacksize);
    }
    else{
        HPGS_ASSERT(!m_cb);
        HPGS_ASSERT(m_state == EXEC);

        Fibre* cur = t_fibre;
        if(cur == this){
            SetThis(nullptr);
        }
    }
    HPGS_LOG_DEBUG(g_logger) << "Fibre::~Fibre id = " << m_id << " total = " << s_fibre_count;
}

/**
 * 重置协程函数，并重置协程状态
 * INIT, TERM, EXCEPT
 */
void Fibre::reset(std::function<void()> cb){
    HPGS_ASSERT(m_stack);
    HPGS_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
    m_cb = cb;
    if(getcontext(&m_ctx)){
        HPGS_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fibre::MainFunc, 0);
    m_state = INIT;
}

void Fibre::createMainFibre(){
    if(t_threadFibre){
        return;
    }
    
    Fibre::ptr main_fibre(new Fibre);
    HPGS_ASSERT(t_fibre == main_fibre.get());
    t_threadFibre = main_fibre;
}

void Fibre::call(){
    SetThis(this);
    m_state = EXEC;
    if(swapcontext(&t_threadFibre->m_ctx, &m_ctx)){
        HPGS_ASSERT2(false, "swapcontext");
    }
}

void Fibre::back(){
    SetThis(t_threadFibre.get());
    if(swapcontext(&m_ctx, &t_threadFibre->m_ctx)){
        HPGS_ASSERT2(false, "swapcontext");
    }
}

void Fibre::swapIn(){
    SetThis(this);
    HPGS_ASSERT(m_state != EXEC);
    m_state = EXEC;
    if(swapcontext(&Scheduler::GetMainFibre()->m_ctx, &m_ctx)){
        HPGS_ASSERT2(false, "swapcontext");
    }
    
    // if(swapcontext(&t_threadFibre->m_ctx, &m_ctx)){
    //     HPGS_ASSERT2(false, "swapcontext");
    // }
}

void Fibre::swapOut(){
    SetThis(Scheduler::GetMainFibre());
    if(swapcontext(&m_ctx, &Scheduler::GetMainFibre()->m_ctx)){
        HPGS_ASSERT2(false, "swapcontext");
    }
    // if(swapcontext(&m_ctx, &t_threadFibre->m_ctx)){
    //     HPGS_ASSERT2(false, "swapcontext");
    // }
}

void Fibre::SetThis(Fibre* f){
    t_fibre = f;
}

Fibre::ptr Fibre::GetThis(){
    if(t_fibre){
        return t_fibre->shared_from_this();
    }

    Fibre::ptr main_fibre(new Fibre);
    HPGS_ASSERT(t_fibre == main_fibre.get());
    t_threadFibre = main_fibre;
    return t_fibre->shared_from_this();
}

void Fibre::YieldToReady(){
    Fibre::ptr cur = GetThis();
    HPGS_ASSERT(cur->m_state == EXEC);
    cur->m_state = READY;
    cur->swapOut();
}

void Fibre::YieldToHold(){
    Fibre::ptr cur = GetThis();
    HPGS_ASSERT(cur->m_state == EXEC);
    cur->m_state = HOLD;
    cur->swapOut();
}

uint64_t Fibre::TotalFibres(){
    return s_fibre_count;
}

void Fibre::MainFunc(){
    Fibre::ptr cur = GetThis();
    HPGS_ASSERT(cur);
    try{
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }
    catch(std::exception& e){
        cur->m_state = EXCEPT;
        HPGS_LOG_ERROR(g_logger) << "Fibre Except" << e.what()
                                 << " fibre_id = " << cur->getId()
                                 << std::endl
                                 << HPGS::BacktraceToString();
    }
    catch(...){
        cur->m_state = EXCEPT;
        HPGS_LOG_ERROR(g_logger) << "Fibre Except"
                                 << " fibre_it = " << cur->getId()
                                 << std::endl
                                 << HPGS::BacktraceToString();
    }

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->swapOut();

    HPGS_ASSERT2(false, "nerver reach fibre_id = " + std::to_string(raw_ptr->getId()));
}

void Fibre::CallerMainFunc(){
    Fibre::ptr cur = GetThis();
    HPGS_ASSERT(cur);
    try{
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }
    catch(std::exception& e){
        cur->m_state = EXCEPT;
        HPGS_LOG_ERROR(g_logger) << "Fibre Except: " << e.what()
                                 << " fibre_id = " << cur->getId()
                                 << std::endl
                                 << HPGS::BacktraceToString();
    }
    catch(...){
        cur->m_state = EXCEPT;
        HPGS_LOG_ERROR(g_logger) << "Fibre Except: "
                                 << " fibre_id = " << cur->getId()
                                 << std::endl
                                 << HPGS::BacktraceToString();
    }

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->back();

    HPGS_ASSERT2(false, "nerver reach fibre_id = " + std::to_string(raw_ptr->getId()));
}

}