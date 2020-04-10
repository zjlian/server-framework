#include "thread.h"
#include "log.h"
#include <assert.h>
#include <exception>
#include <unistd.h>

namespace zjl
{

/**
 * 线程局部变量
*/
// 记录当前线程的 Thread 实例的指针
// static thread_local Thread* t_thread = nullptr;
static thread_local pid_t t_tid = 0;
// 记录当前线程的线程名
static thread_local std::string t_thread_name = "UNKNOWN";

static Logger::ptr system_logger = GET_LOGGER("system");

/**
 * =========================================
 * Semaphore 类的实现
 * =========================================
*/

Semaphore::Semaphore(uint32_t count)
{
    if (sem_init(&m_semaphore, 0, count))
    {
        LOG_FATAL(
            system_logger,
            "sem_init() 初始化信号量失败");
        throw std::system_error();
    }
}

Semaphore::~Semaphore()
{
    sem_destroy(&m_semaphore);
}

void Semaphore::wait()
{
    if (sem_wait(&m_semaphore))
    {
        LOG_FATAL(
            system_logger,
            "sem_wait() 异常");
        throw std::system_error();
        // TODO 失败时是否应该直接结束程序？
    }
}

void Semaphore::notify()
{
    if (sem_post(&m_semaphore))
    {
        LOG_FATAL(
            system_logger,
            "sem_post() 异常");
        throw std::system_error();
        // TODO 失败时是否应该直接结束程序？
    }
}

/**
 * @brief 线程数据类
 * 封装线程执行需要的数据
*/
struct ThreadData
{
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc m_callback;
    std::string m_name;
    pid_t* m_id;
    Semaphore* m_semaphore;

    ThreadData(ThreadFunc func,
               const std::string& name,
               pid_t* tid,
               Semaphore* sem)
        : m_callback(std::move(func)),
          m_name(name),
          m_id(tid),
          m_semaphore(sem) {}

    void runInThread()
    {
        // 获取系统线程 id
        *m_id = GetThreadID();
        m_id = nullptr;
        // 信号量 +1，通知主线程，子线程启动成功
        m_semaphore->notify();
        m_semaphore = nullptr;
        t_tid = GetThreadID();
        t_thread_name = m_name.empty() ? "UNKNOWN" : m_name;
        pthread_setname_np(pthread_self(), m_name.substr(0, 15).c_str());
        try
        {
            m_callback();
        }
        catch (const std::exception& e)
        {
            LOG_FMT_FATAL(
                system_logger,
                "线程执行异常，name = %s, 原因：%s",
                m_name.c_str(),
                e.what());
            abort();
        }
    }
};

/**
 * ===================================================
 * Thread 类的实现
 * ===================================================
*/

pid_t Thread::GetThisId()
{
    return t_tid;
}

const std::string&
Thread::GetThisThreadName()
{
    return t_thread_name;
}

void Thread::SetThisThreadName(const std::string& name)
{
    t_thread_name = name;
}

Thread::Thread(ThreadFunc callback, const std::string& name)
    : m_id(-1),
      m_name(name),
      m_thread(0),
      m_callback(callback),
      m_semaphore(0),
      m_started(true),
      m_joined(false)
{
    // 调用 pthread_create 创建新线程
    ThreadData* data =
        new ThreadData(m_callback, m_name, &m_id, &m_semaphore);
    int result = pthread_create(&m_thread, nullptr, &Thread::Run, data);
    if (result)
    {
        m_started = false;
        delete data;
        LOG_FMT_FATAL(
            system_logger,
            "pthread_create() 线程创建失败, 线程名 = %s, 错误码 = %d",
            name.c_str(), result);
        throw std::system_error();
    }
    else
    {
        // 等待子线程启动
        m_semaphore.wait();
        // m_id 储存系统线程 id, 如果小于0，说明线程启动失败
        assert(m_id > 0);
    }
}

Thread::~Thread()
{
    // 如果线程有效且位 join，将线程与主线程分离
    if (m_started && !m_joined)
    {
        pthread_detach(m_thread);
    }
}

pid_t Thread::getId() const
{
    return m_id;
}

const std::string&
Thread::getName() const
{
    return m_name;
}

void Thread::setName(const std::string& name)
{
    m_name = name;
}

int Thread::join()
{
    assert(m_started);
    assert(!m_joined);
    m_joined = true;
    return pthread_join(m_thread, nullptr);
}

void* Thread::Run(void* arg)
{
    std::unique_ptr<ThreadData> data((ThreadData*)arg);
    data->runInThread();
    return 0;
}
}
