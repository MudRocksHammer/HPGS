#ifndef __HPGS_SOCKET_H__
#define __HPGS_SOCKET_H__


#include <memory>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "noncopyable.h"
//#include "address.h"


namespace HPGS{

/** 
 * @socket封装
 */
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable{
public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    /**
     * @brief Socket类型
     */
    enum Type{
        //TCP类型
        TCP = SOCK_STREAM,
        //UDP类型
        UDP = SOCK_DGRAM
    };

    /**
     * @brief Socket协议簇
     */
    enum Family{
        //IPv4 socket
        IPv4 = AF_INET,
        //IPv6 socket
        IPv6 = AF_INET6,
        //Unix socket
        UNIX = AF_UNIX
    };

    /**
     * @brief 创建TCP Socket(满足地址类型)
     * @param[in] address 地址
     */

};

}

#endif