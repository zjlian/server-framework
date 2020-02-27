#ifndef SERVER_FRAMEWORK_THREAD_H
#define SERVER_FRAMEWORK_THREAD_H

#include "util.h"
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

namespace zjl {

/**
 * @brief 对 semaphore.h 信号量的简单封装
*/
class Semaphore : public noncopyable {
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
 * @brief 线程类
 * 基于 pthread 封装的
*/
class Thread : public noncopyable {
public:
    typedef std::shared_ptr<Thread> ptr;
    typedef std::unique_ptr<Thread> uptr;
    typedef std::function<void()> ThreadFunc;

    Thread(ThreadFunc callback, std::string name);
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
