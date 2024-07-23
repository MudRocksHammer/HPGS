#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>

namespace HPGS{

static HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");

enum EpollCtlOp{

};

static std::ostream& operator<< (std::ostream& os, const EpollCtlOp& op){
    switch((int)op){
#define XX(ctl) \
        case ctl: \
            return os << #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
        default:
            return os << (int)op;
    }
#undef XX
}


static std::ostream& operator<< (std::ostream& os, EPOLL_EVENTS events){
    if(!events){
        return os << "0";
    }
    bool first = true;
#define XX(E) \
    if(events & E){ \
        if(!first){ \
            os << "|"; \
        } \
        os << #E; \
        first = false; \
    }
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLRDBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return os;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event){
    switch(event){
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            HPGS_ASSERT2(false, "getContext");
    }
    throw std::invalid_argument("getContext& ctx");
}

void IOManager::FdContext::resetContext(EventContext& ctx){
    ctx.scheduler = nullptr;
    ctx.fibre.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event){
    HPGS_ASSERT(events & event);

    events = (Event)(events & ~event);
    EventContext& ctx = getContext(event);
    if(ctx.cb){
        ctx.scheduler->schedule(&ctx.cb);
    }
    else{
        ctx.scheduler->schedule(&ctx.fibre);
    }
    ctx.scheduler = nullptr;
    return;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
: Scheduler(threads, use_caller, name) {
    m_epfd = epoll_create(5000);
    HPGS_ASSERT(m_epfd > 0);

    //创建管道，m_tickleFds[0]是读端，m_tickleFds[1]是写端
    int rt = pipe(m_tickleFds);
    HPGS_ASSERT(!rt);

    //注册pipe fd 可读事件，用与tickle调度携程，通过epoll_event.data.fd保存
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;   //边沿触发
    event.data.fd = m_tickleFds[0];

    //非阻塞方式
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    HPGS_ASSERT(!rt);

    //将管道的读描述符加入epoll多路复用，如果管道可读，idle中的epoll_wait会返回
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    HPGS_ASSERT(!rt);

    //初始化fdContext
    contextResize(32);
    HPGS_LOG_INFO(g_logger) << "create iomanager succeed";

    //开启调度器
    start();
}

IOManager::~IOManager(){
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0; i < m_fdContexts.size(); i++){
        if(m_fdContexts[i]){
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size){
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); i++){
        if(!m_fdContexts[i]){
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb){
    //找到fd对应的Fdcontext，如果不存在，分配一个
    FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd){
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    }
    else{
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        //没找到说明fd用完了，扩容1.5
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    //同一个fd不允许重复添加相同事件
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(HPGS_UNLIKELY(fd_ctx->events & event)){
        HPGS_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd 
                                 << " event = " << (EPOLL_EVENTS)event
                                 << " fd_ctx.event = " << (EPOLL_EVENTS)fd_ctx->events;
        HPGS_ASSERT(!(fd_ctx->events & event));
    }

    //将新的事件加入到epoll_wait,使用epoll_event的私有指针存储fdContext的位置
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        HPGS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                 << (EpollCtlOp)op << ", " << fd << ", "
                                 << (EPOLL_EVENTS)epevent.events << "):"
                                 << rt << " (" << errno << ") (" <<strerror(errno)
                                 << ") fd_ctx->events = " << (EPOLL_EVENTS)fd_ctx->events;
        return -1;
    }

    ++m_pendingEventCount;
    //找到这个fd的event事件对应的EventContext，对其中的scheduler,cb,fibre进行赋值
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    //为什么scheduler，fiber，callback要为空
    HPGS_ASSERT(!event_ctx.scheduler && !event_ctx.fibre && !event_ctx.cb);

    //IOManager继承的scheduler，赋值scheduler和回调函数，如果回调函数为空，则把当前协程当成回调执行体
    event_ctx.scheduler = Scheduler::GetThis();
    if(cb){
        event_ctx.cb.swap(cb);
    } 
    else{
        event_ctx.fibre = Fibre::GetThis();
        HPGS_ASSERT2(event_ctx.fibre->getState() == Fibre::EXEC
                    , "state = " << event_ctx.fibre->getState());
    }

    return 0;
}

bool IOManager::delEvent(int fd, Event event){
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(HPGS_UNLIKELY(!(fd_ctx->events & event))){
        return false;
    }

    //判断除了要修改的event还有没有别的event留着，若没有就是DEL event
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        HPGS_LOG_ERROR(g_logger) << "epoll_ctl(" << "m_epfd " << ", "
                                 << "(EpollCtlOp)op" << ", " << fd << ", "
                                 << (EPOLL_EVENTS)epevent.events << "):"
                                 << rt << " (" << errno << ") (" <<strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);

    return true;
}

bool IOManager::cancelEvent(int fd, Event event){
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    //没有这个event
    if(HPGS_UNLIKELY(!(fd_ctx->events & event))){
        return false;
    }

    Event new_events = (Event)(fd_ctx->events &  ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        HPGS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                 << (EpollCtlOp)op << ", " << fd << ", "
                                 << (EPOLL_EVENTS)epevent.events << "):"
                                 << rt << " (" << errno << ") (" 
                                 << strerror(errno) << ")";
        return false;     
    }

    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd){
    RWMutex::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events){
        return false;
    }
        

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        HPGS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                 << (EpollCtlOp)op << ", " << fd << ", "
                                 << (EPOLL_EVENTS)epevent.events << "):"
                                 << rt << " (" << errno << ") (" 
                                 << strerror(errno) << ")";
        return false; 
    }

    if(fd_ctx->events & READ){
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }
    if(fd_ctx->events & WRITE){
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    HPGS_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetThis(){
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle(){
    if(!hasIdleThreads()){
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    HPGS_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout){
    timeout = getNextTimer();
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping(){
    uint64_t timeout = 0;
    return stopping(timeout);
}

/**
 * @details 对于io协程调度来说，idle时应阻塞在等待IO事件上，idle退出的时机是epoll_wait返回，
 * 对应的操作是tickle或注册的IO事件就绪。调度器没有任务时会阻塞在idle协程上，对IO调度器来说，
 * idle状态应该关注两件事，一是有没有新的调度任务发生，对应Scheduler::schedule()函数，
 * 如果有新的任务，应该立即退出idle执行新的任务；二是关注当前注册的所有IO事件有没有触发，
 * 如果有触发，应该执行对应IO事件的回调函数
 */
void IOManager::idle(){
    HPGS_LOG_DEBUG(g_logger) << "idle";
    //一次epoll_wait最多检测256个就绪事件，如果就绪事件超过了这个数，那么会在下轮epoll_wait处理
    const uint64_t MAX_EVENTS = 256;
    epoll_event* events = new epoll_event[MAX_EVENTS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });

    while(true){
        uint64_t next_timeout = 0;
        if(HPGS_UNLIKELY(stopping(next_timeout))){
            HPGS_LOG_INFO(g_logger) << "name = " << getName()
                                    << " idle stopping exit";
            break;
        }
        else{
            //HPGS_LOG_INFO(g_logger) << "idle";
        }

        int rt = 0;
        do{
            static const int MAX_TIMEOUT = 3000;
            //最小timeout
            if(next_timeout != ~0ull){
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            }
            else{
                next_timeout = MAX_TIMEOUT;
            }
            //阻塞在epoll_wait上，等待事件发生
            rt = epoll_wait(m_epfd, events, MAX_EVENTS, (int)next_timeout);
            if(rt < 0 && errno == EINTR){
                //epoll_wait 发生错误
            }
            else{
                //收到事件或正常超时
                break;
            }
        }while(true);

        //找到定时器中所有需要执行的任务加入到schedule
        std::vector<std::function<void()> > cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()){
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        //遍历IO事件
        for(int i = 0; i < rt; i++){
            epoll_event& event = events[i];
            //tickle[0]用于通知协程调度，这时只需要把管道里的内容读取完毕，
            //本轮idle结束，通过Scheduler::run()执行任务
            if(event.data.fd == m_tickleFds[0]){
                uint8_t dummy[256];
                while(read(m_tickleFds[0], dummy, sizeof(dummy)) > 0);
                continue;
            }

            //通过epoll_event的私有指针获取Fdcontext
            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            //EPOLLERR:出错，EPOLLHUP:套接字关闭,出现这两种事件，
            //应该同时触发fd读和写事件，否则可能出现注册的事件永远执行不到的情况
            if(event.events & (EPOLLERR | EPOLLHUP)){
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            int real_events = NONE;
            if(event.events & EPOLLIN){
                real_events |= READ;
            }
            if(event.events & EPOLLOUT){
                real_events |= WRITE;
            }

            if((fd_ctx->events & real_events) == NONE){
                continue;
            }

            //除去已发生的事件，将剩下的事件重新加入epoll_wait
            //如果剩下的事件为0，表示这个fd已经不需要关注了，从epoll中删除
            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2){
                HPGS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                         << (EpollCtlOp)op << ", " << fd_ctx->fd
                                         << ", " << (EPOLL_EVENTS)event.events
                                         << "):" << rt2 << " (" << errno
                                         << ") (" << strerror(errno) << ")";
                continue;
            }

            //处理已发生的事件，也就是让调度器调度指定的函数或协程
            if(real_events & READ){
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if(real_events & WRITE){
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }// end for 

        //不idle了换出去
        Fibre::ptr cur = Fibre::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();

    }//end while(true)
}

void IOManager::onTimerInsertAtFront(){
    tickle();
}

}