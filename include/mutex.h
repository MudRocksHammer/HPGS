#ifndef __HPGS_MUTEX_H__
#define __HPGS_MUTEX_H__


#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include <atomic>
#include <list>

#include "noncopyable.h"
//#include "fibre.h"


namespace HPGS{

class Semaphore : Noncopyable {
public:
    /**
     * @brief 构造函数
     * @param[in] count 信号量值大小
     */
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void notify();

private:
    sem_t m_semaphore;
};

/**
 * @brief 局部锁的模板实现
 */
template<class T>
struct ScopedLockImpl{
public:
    ScopeLockImpl(T& mutex) : m_mutex(mutex){
        m_mutex.lock();
        m_locked = true;
    }
    //析构自动释放锁
    ~ScopeLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

/**
 * @brief 局部读锁的模板实现
 * readlock被持有时若有线程想要修改资源需要等待readlock释放
 */
template<class T>
struct ReadScopedLockImpl{
public:
    ReadScopeLockImpl(T& mutex) : m_mutex(mutex){
        m_mutex.rdlock();
        m_locked = true;
    }
    //析构自动释放锁
    ~ReadScopeLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

/**
 * @brief 局部写锁模板实现
 */
template<class T>
struct WriteScopedLockImpl{
public:
    WriteScopeLockImpl(T& mutex) : m_mutex(mutex){
        m_mutex.wrlock();
        m_locked = true;
    }
    //析构自动释放锁
    ~WriteScopeLockImpl(){
        unlock();
    }

    void lock(){
        if(!m_locked){
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

class Mutex : Noncopyable{
public:
    typedef ScopedLockImpl<Mutex> Lock;

    Mutex(){
        pthread_mutex_init(&m_mutex, nullptr);
    }
    ~Mutex(){
        pthread_mutex_destroy(&m_mutex);
    }

    void unlock(){
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};

class RWMutex : Noncopyable{
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    RWMutex(){
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex(){
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock(){
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock(){
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock(){
        pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

class Spinlock : Noncopyable{
public:
    typedef ScopedLockImpl<Spinlock> Lock;

    Spinlock(){
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock(){
        pthread_spin_destroy(&m_mutex);
    }

    void lock(){
        pthread_spin_lock(&m_mutex);
    }

    void unlock(){
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex;
};

class CASLock : Noncopyable{
public:
    typedef ScopedLockImpl<CASLock> Lock;

    CASLock(){
        m_mutex.clear();
    }

    ~CASLock(){

    }

    void lock(){
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    void unlock(){
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }

private:
    volatile std::atomic_flag m_mutex;
};

class Scheduler;
class FibreSemaphore : Noncopyable{
public:
    typedef Spinlock MutexType;

    FibreSemaphore(size_t initail_concurrency = 0);
    ~FibreSemaphore();

    bool tryWait();
    void wait();
    void notify();

    size_t getConcurrency() const { return m_concurrency; }
    void reset() { m_concurrency = 0; }

private:
    MutexType m_mutex;
    //std::list<std::pair<Scheduler*, Fibre::ptr> > m_waiters;
    size_t m_concurrency;
};

}

#endif