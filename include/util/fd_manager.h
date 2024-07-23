#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__


#include <memory>
#include <vector>
#include "thread.h"
#include "singleton.h"


namespace HPGS{

/**
 * @brief 文件fd上下文类
 * @details 管理fd类型(是否是socket)，是否阻塞，是否关闭，读写超时时间 
 */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    typedef std::shared_ptr<FdCtx> ptr;

    //通过文件fd构造上下文类
    FdCtx(int fd);

    ~FdCtx();

    /**
     * @brief 是否初始化完成
     */
    bool isInit() const { return m_isInit; }

    /**
     * @brief 是否是socket
     */
    bool isSocket() const { return m_isSocket; }

    /**
     * @brief 是否已关闭
     */
    bool isClose() const { return m_isClosed; }

    /**
     * @brief 设置用户，主动设置非阻塞
     * @param[in] v 是否阻塞
     */
    void setUserNonblock(bool v) { m_userNonblock = v; }

    /**
     * @brief 获取是否用户主动设置非阻塞
     */
    bool getUserNonblock() const { return m_userNonblock; }

    /**
     * @brief 设置系统非阻塞
     * @param[in] v 是否阻塞
     */
    void setSysNonblock(bool v) { m_sysNonblock = v; }

    /**
     * @brief 获取系统非阻塞
     */
    bool getSysNonblock() const { return m_sysNonblock; }

    /**
     * @brief 设置超时时间
     * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @param[in] v 时间毫秒
     */
    void setTimeout(int type, uint64_t v);

    /**
     * @brief 获取超时时间
     * @param[in] 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @return 超时时间毫秒
     */
    uint64_t getTimeout(int type);

private:
    bool init();

private:
    //声明带有位域的布尔型变量，表示这个变量将用1位来存储
    //是否初始化
    bool m_isInit: 1;
    //是否socket
    bool m_isSocket: 1;
    //是否hook非阻塞
    bool m_sysNonblock: 1;
    //是否用户主动设置非阻塞
    bool m_userNonblock: 1;
    //是否关闭
    bool m_isClosed: 1;
    //fd
    int m_fd;
    //读超时时间毫秒
    uint64_t m_recvTimeout;
    //写超时时间毫秒
    uint64_t m_sendTimeout;

};

/**
 * @brief fd管理类
 */
class FdManager {
public:
    typedef RWMutex RWMutexType;

    /**
     * @brief 无参构造函数
     */
    FdManager();

    /**
     * @brief 获取/创建文件fd类FdCtx
     * @param[in] fd 
     * @param[in] auto_create 是否自动创建
     * @return 返回对应fd上下文类FdCtx::ptr
     */
    FdCtx::ptr get(int fd, bool auto_create = false);

    /**
     * @brief 删除文件fd上下文类
     * @param[in] fd
     */
    void del(int fd);

private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_datas;
};

//fd管理器单例
typedef Singleton<FdManager> fdMgr;

}

#endif