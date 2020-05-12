#ifndef SERVER_FRAMEWORK_THREAD_H
#define SERVER_FRAMEWORK_THREAD_H

#include "util.h"
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <cstdint>
#include <sys/types.h>
#include <unistd.h>

namespace zjl
{

/**
 * @brief 对 semaphore.h 信号量的简单封装
*/
class Semaphore : public noncopyable
{
public:
    explicit Semaphore(uint32_t count);
    ~Semaphore();
    // -1，值为零时阻塞
    void wait();
    //  +1
    void notify();

private:
    sem_t m_semaphore;
};

/**
 * @brief 作用域线程锁包装器
 * T 需要实现 lock() 与 unlock() 方法
*/
template <typename T>
class ScopedLockImpl
{
public:
    explicit ScopedLockImpl(T* mutex)
        : m_mutex(mutex)
    {
        m_mutex->lock();
        m_locked = true;
    }

    ~ScopedLockImpl() { unlock(); }

    void lock()
    {
        if (!m_locked)
        {
            m_mutex->lock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked)
        {
            m_locked = false;
            m_mutex->unlock();
        }
    }

private:
    T* m_mutex;
    bool m_locked;
};

/**
 * @brief 作用域读写锁包装器
 * T 需要实现 readLock() 与 unlock() 方法
*/
template <typename T>
class ReadScopedLockImpl
{
public:
    explicit ReadScopedLockImpl(T* mutex)
        : m_mutex(mutex)
    {
        m_mutex->readLock();
        m_locked = true;
    }

    ~ReadScopedLockImpl() { unlock(); }

    void lock()
    {
        if (!m_locked)
        {
            m_mutex->readLock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked)
        {
            m_locked = false;
            m_mutex->unlock();
        }
    }

private:
    T* m_mutex;
    bool m_locked;
};

/**
 * @brief 作用域读写锁包装器
 * T 需要实现 writeLock() 与 unlock() 方法
*/
template <typename T>
class WriteScopedLockImpl
{
public:
    explicit WriteScopedLockImpl(T* mutex)
        : m_mutex(mutex)
    {
        m_mutex->writeLock();
        m_locked = true;
    }

    ~WriteScopedLockImpl() { unlock(); }

    void lock()
    {
        if (!m_locked)
        {
            m_mutex->writeLock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked)
        {
            m_locked = false;
            m_mutex->unlock();
        }
    }

private:
    T* m_mutex;
    bool m_locked;
};

/**
 * @brief pthread 互斥量的封装
*/
class Mutex
{
public:
    Mutex()
    {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    int lock()
    {
        return pthread_mutex_lock(&m_mutex);
    }

    int unlock()
    {
        return pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex{};
};

/**
 * @brief 互斥量的 RAII
*/
using ScopedLock = ScopedLockImpl<Mutex>;

/**
 * @brief pthread 读写锁的封装
*/
class RWLock
{
public:
    RWLock()
    {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWLock()
    {
        pthread_rwlock_destroy(&m_lock);
    }

    int readLock()
    {
        return pthread_rwlock_rdlock(&m_lock);
    }

    int writeLock()
    {
        return pthread_rwlock_wrlock(&m_lock);
    }

    int unlock()
    {
        return pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock{};
};

/**
 * @brief 读写锁针对读操作的作用域 RAII 实现
*/
using ReadScopedLock = ReadScopedLockImpl<RWLock>;

/**
 * @brief 读写锁针对写操作的作用域 RAII 实现
*/
using WriteScopedLock = WriteScopedLockImpl<RWLock>;

/**
 * @brief 线程类
 * 基于 pthread 封装的
*/
class Thread : public noncopyable
{
public:
    typedef std::shared_ptr<Thread> ptr;
    typedef std::unique_ptr<Thread> uptr;
    typedef std::function<void()> ThreadFunc;

    Thread(ThreadFunc callback, const std::string& name);
    ~Thread();
    // 获取线程 id
    pid_t getId() const;
    // 获取线程名称
    const std::string& getName() const;
    // 设置线程名称
    void setName(const std::string& name);
    // 将线程并入主线程
    int join();

public:
    // 获取当前线程
    // static Thread* GetThis();
    // 获取当前线程的系统线程 id
    static pid_t GetThisId();
    // 获取当前运行线程的名称
    static const std::string& GetThisThreadName();
    // 设置当前运行线程的名称
    static void SetThisThreadName(const std::string& name);
    // 启动线程, 接收 Thread*
    static void* Run(void* arg);

private:
    // 禁用 Thread 类的拷贝移动等操作
    // Thread(const Thread&) = delete;
    // Thread(const Thread&&) = delete;
    // Thread& operator=(const Thread&) = delete;
    // Thread& operator=(const Thread&&) = delete;

private:
    // 系统线程 id, 通过 syscall() 获取
    pid_t m_id;
    // 线程名称
    std::string m_name;
    // pthread 线程 id
    pthread_t m_thread;
    // 线程执行的函数
    ThreadFunc m_callback;
    // 控制线程启动的信号量
    Semaphore m_semaphore;
    // 线程状态
    bool m_started;
    bool m_joined;
};
}

#endif
