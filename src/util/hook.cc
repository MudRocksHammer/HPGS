#include "hook.h"
#include <dlfcn.h>

#include "config.h"
#include "log.h"
#include "fibre.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"

HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");

namespace HPGS{

static HPGS::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    HPGS::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

//使用自定义函数
static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init(){
    static bool is_inited = false;
    if(is_inited){
        return;
    }
    /**
     * 将函数名称链接'_f'作为函数指针变量名，
     * 并调用'dlsym'来查找下一个共享对象中的符号(函数名),返回一个指向该符号的指针
     * 使用HOOK_FUN(XX)展开执行XX宏定义，将会对所有列出的函数hook进行初始化
     * name ## _f 表示宏拼接操作，将name和_f拼接在一起形成新的标识符
     */
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
    _HookIniter(){
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();

        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
            HPGS_LOG_INFO(g_logger) << "tcp connect timeout changed from "  
                                    << old_value << " to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

//hook_init() 放在一个静态对象的构造函数中调用，这表示在main函数运行之前就会获取各个符号的地址并保存在全局变量中。
static _HookIniter s_hook_initer;

bool is_hook_enable(){
    return t_hook_enable;
}

void set_hook_enable(bool flag){
    t_hook_enable = flag;
}

}

struct timer_info{
    int cancelled = 0;
};

/**
 * @brief do_io模板将accept，read，write，recv，send等IO操作hook实现
 */
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name
                    , uint32_t event, int timeout_so, Args&&... args){
    if(!HPGS::t_hook_enable){
        return fun(fd, std::forward<Args>(args)...);
    }

    HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(fd);
    if(!ctx){
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClose()){
        errno = EBADF;
        return -1;
    }

    //系统fd都是非阻塞的，或则用户fd支持非阻塞
    if(!ctx->isSocket() || ctx->getUserNonblock()){
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR){
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(n == -1 && errno == EAGAIN){
        HPGS::IOManager* iom = HPGS::IOManager::GetThis();
        HPGS::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if(to != (uint64_t)-1){
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();
                if(!t || t->cancelled){
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (HPGS::IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (HPGS::IOManager::Event)(event));
        if(HPGS_UNLIKELY(rt)){
            HPGS_LOG_ERROR(g_logger) << hook_fun_name << " addEvent(" 
                                     << fd << ", " << event << ")";
            if(timer){
                timer->cancel();
            }
            return -1;
        }
        else{
            HPGS::Fibre::YieldToHold();
            if(timer){
                timer->cancel();
            }
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }
            goto retry;
        }
    }

    return n;
}

extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds){
    if(!HPGS::t_hook_enable){
        return sleep_f(seconds);
    }

    HPGS::Fibre::ptr fibre = HPGS::Fibre::GetThis();
    HPGS::IOManager* iom = HPGS::IOManager::GetThis();
    iom->addTimer(seconds * 1000, std::bind(
        //定义了一个成员函数指针类型，该成员函数属于Scheduler类，接受Fibre::ptr类型的指针和int型参数，返回类型为void
        (void(HPGS::Scheduler::*)(HPGS::Fibre::ptr, int thread))&HPGS::IOManager::schedule, iom, fibre, -1
        
    ));
    HPGS::Fibre::YieldToHold();
    return 0;
}

int usleep(useconds_t usec){
    if(!HPGS::t_hook_enable){
        return usleep_f(usec);
    }

    HPGS::Fibre::ptr fibre = HPGS::Fibre::GetThis();
    HPGS::IOManager* iom = HPGS::IOManager::GetThis();
    iom->addTimer(usec / 1000, std::bind(
        (void(HPGS::Scheduler::*)(HPGS::Fibre::ptr, int))&HPGS::IOManager::schedule, iom, fibre, -1
    ));
    HPGS::Fibre::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem){
    if(!HPGS::t_hook_enable){
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    HPGS::Fibre::ptr fibre = HPGS::Fibre::GetThis();
    HPGS::IOManager* iom = HPGS::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind(
        (void(HPGS::Scheduler::*)(HPGS::Fibre::ptr, int))&HPGS::IOManager::schedule, iom, fibre, -1
    ));
    HPGS::Fibre::YieldToHold();
    return 0;
}

int socket(int domain, int type, int protocol){
    if(!HPGS::t_hook_enable){
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1){
        return fd;
    }
    HPGS::fdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms){
    if(!HPGS::t_hook_enable){
        return connect_f(fd, addr, addrlen);
    }
    HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose()){
        errno = EBADF;
        return -1;
    }

    //不为套接字
    if(!ctx->isSocket()){
        return connect_f(fd, addr, addrlen);
    }

    //非阻塞
    if(ctx->getUserNonblock()){
        return connect_f(fd, addr, addrlen);
    }

    //套接字是非阻塞的，这里会返回EINPROGRESS错误
    int n = connect_f(fd, addr, addrlen);
    if(n == 0){
        return 0;
    }
    else if(n != -1 || errno != EINPROGRESS){
        return n;
    }

    HPGS::IOManager* iom = HPGS::IOManager::GetThis();
    HPGS::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    //非阻塞
    if(timeout_ms != (uint64_t)-1){
        //设置一个timeout的定时器，等待连接时间yield
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom](){
            auto t = winfo.lock();
            if(!t || t->cancelled){
                return;
            }
            t->cancelled = ETIMEDOUT;
            //定时器触发则等待超时，取消write事件
            iom->cancelEvent(fd, HPGS::IOManager::WRITE);
        }, winfo);
    }

    //添加write事件并yield，等待超时或socket可写，如果先超时，触发write事件，协程从yield点返回，通过超时标志设置errno并返回-1
    //如果超时前可写了，则取消定时器，
    int rt = iom->addEvent(fd, HPGS::IOManager::WRITE);
    if(rt == 0){
        HPGS::Fibre::YieldToHold();
        if(timer){
            timer->cancel();
        }
        if(tinfo->cancelled){
            errno = tinfo->cancelled;
            return -1;
        }
    }
    else{
        if(timer){
            timer->cancel();
        }
        HPGS_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)){
        return -1;
    }
    if(!error){
        return 0;
    }
    else{
        errno = error;
        return -1;
    }   
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen){
    return connect_with_timeout(sockfd, addr, addrlen, HPGS::s_connect_timeout);
}

int accept(int s, struct sockaddr* addr, socklen_t* addrlen){
    int fd = do_io(s, accept_f, "accept", HPGS::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0){
        HPGS::fdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void* buf, size_t count){
    return do_io(fd, read_f, "read", HPGS::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt){
    return do_io(fd, readv_f, "readv", HPGS::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags){
    return do_io(sockfd, recv_f, "recv", HPGS::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen){
    return do_io(sockfd, recvfrom_f, "recvfrom", HPGS::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags){
    return do_io(sockfd, recvmsg_f, "recvmsg", HPGS::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count){
    return do_io(fd, write_f, "write", HPGS::IOManager::WRITE, SO_RCVTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", HPGS::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", HPGS::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", HPGS::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", HPGS::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd){
    if(!HPGS::t_hook_enable){
        return close_f(fd);
    }

    HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(fd);
    if(ctx){
        auto iom = HPGS::IOManager::GetThis();
        if(iom){
            iom->cancelAll(fd);
        }
        HPGS::fdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ...){
    va_list va;
    va_start(va, cmd);
    switch(cmd){
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()){
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()){
                    arg |= O_NONBLOCK;
                }
                else{
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()){
                    return arg;
                }
                if(ctx->getUserNonblock()){
                    return arg | O_NONBLOCK;
                }
                else{
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif  
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...){
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request){
        bool user_nonblock = !!*(int*)arg;
        HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()){
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen){
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen){
    if(!HPGS::t_hook_enable){
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET){
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO){
            HPGS::FdCtx::ptr ctx = HPGS::fdMgr::GetInstance()->get(sockfd);
            if(ctx){
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}
