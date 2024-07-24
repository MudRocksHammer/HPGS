#include "socket.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "macro.h"
#include "hook.h"
#include <limits>

namespace HPGS{

static HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");

Socket::ptr Socket::CreateTcp(HPGS::Address::ptr address){
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUdp(HPGS::Address::ptr address){
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTcpSocket(){
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUdpSocket(){
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTcpSocket6(){
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUdpSocket6(){
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    Socket::ptr sock(new Socket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    Socket::ptr sock(new Socket(UNIX, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol) : m_sock(-1), m_family(family)
, m_type(type), m_protocol(protocol), m_isConnected(false){

}

Socket::~Socket(){
    close();
}

int64_t Socket::getSendTimeout(){
    FdCtx::ptr ctx = fdMgr::GetInstance()->get(m_sock);
    if(ctx){
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t v){
    struct timeval tv { int(v / 1000), int(v % 1000 * 1000) };
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout(){
    FdCtx::ptr ctx = fdMgr::GetInstance()->get(m_sock);
    if(ctx){
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t v){
    struct timeval tv { int(v / 1000), int(v % 1000 * 1000) };
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, sockelen_t* len){
    int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
    if(rt){
        HPGS_LOG_INFO(g_logger) << "getOption sock = " << m_sock
                                << " level = " << level << " option = "
                                << option << " errno = " << errno
                                << " errstr = " << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len){
    if(setsockopt(m_sock, level, option, result, (socklen_t)len)){
        HPGS_LOG_DEBUG(g_logger) << "setOption sock = " << m_sock << " level = "
                                 << level << " option = " << option << " errno = "
                                 << errno << " errstr = " << strerror(errno);
        return false;
    }
    return true;
}

Socket::ptr Socket::accept(){
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if(newsock == -1){
        HPGS_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno"
                                 << errno << " errstr = " << strerror(errno);
        return nullptr;
    }
    if(sock->init(newsock)){
        return sock;
    }
    return nullptr;
}

bool Socket::init(int sock){
    FdCtx::ptr ctx = fdMgr::GetInstance()->get(sock);
    if(ctx && ctx->isSocket() && !ctx->isClose()){
        m_sock = sock;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemotrAddress();
        return true;
    }
    return false;
}

bool Socket::bind(const Address::ptr addr){
    if(!isValid()){
        newSock();
        if(HPGS_UNLIKELY(!isValid())){
            return false;
        }
    }

    if(HPGS_UNLIKELY(addr->getFamily() != m_family)){
        HPGS_LOG_ERROR(g_logger) << "bind sock.family(" << m_family
                                 << ") addr.family(" << addr->getFamily()
                                 << ") not equal, addr = " << addr->toString();
        return false;
    }

    UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
    if(uaddr){
        Socket::ptr sock = Socket::CreateUnixTcpSocket();
        if(sock->connect(uaddr)){
            return false;
        }
        else{
            HPGS::FSUtil::Unlink(uaddr->getPath(), true);
        }
    }

    if(::bind(m_sock, addr->getAddr(), addr->getAddrLen())){
        HPGS_LOG_ERROR(g_logger) << "bind error errno = " << errno  
                                 << " errstr = " strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

}