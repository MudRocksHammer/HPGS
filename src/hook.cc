#include "hook.h"
#include <dlfcn.h>

#include "config.h"
#include "log.h"
#include "fibre.h"
//#include "iomanager.h"
//#include "fd_manager.h"
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

static _HookIniter s_hook_initer;

bool is_hook_enable(){
    return t_hook_enable;
}

void set_hook_enable(bool flag){
    t_hook_enable = flag;
}

}

struct timer_info{
    int canceller = 0;
};

template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name
                    , uint32_t event, int timeout_so, Args&&... args){
    if(!HPGS::t_hook_enable){
        return fun(fd, std::forward<Args>(args)...);
    }

    //HPGS::FdCtx::ptr ctx = HPGS::FdMgr::GetInstance()->get(fd);
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
    //HPGS::IOManager* iom = HPGS::IOManager::GetThis();
    return 0;
}

}
