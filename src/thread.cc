#include "thread.h"
#include "log.h"
#include <exception>
#include <unistd.h>

namespace zjl {

/**
 * 线程局部变量
*/
// 记录当前线程的 Thread 实例的指针
static thread_local Thread* t_thread = nullptr;
// 记录当前线程的线程名
static thread_local std::string t_thread_name = "UNKNOWN";

static Logger::ptr system_logger = GET_LOGGER("system");

/**
 * Thread 类的实现
*/
Thread*
Thread::GetThis() {
    return t_thread;
}

const std::string&
Thread::GetThisThreadName() {
    return t_thread_name;
}

void SetThisThreadName(const std::string& name) {
    if (t_thread) {
        t_thread->setName(name);
    }
    t_thread_name = std::move(name);
}

Thread::Thread(std::function<void()> callback, std::string name)
    : m_id(-1), m_name(name), m_thread(0), m_callback(callback) {
    // 调用 pthread_create 创建新线程
    // 将 Thread 实例作为参数传递给静态方法 Thread::Run, 在静态方法内部调用 Thread 实例的 callback
    int result = pthread_create(&m_thread, nullptr, &Thread::Run, this);
    if (result) {
        LOG_FMT_ERROR(
            system_logger,
            "pthread_create() 线程创建失败, 线程名 = %s, 错误码 = %d",
            name.c_str(), result);
        throw std::system_error();
    }
}

Thread::~Thread() {
    // 如果线程存在，将线程与主线程分离
    if (m_thread) {
        pthread_detach(m_thread);
    }
    // TODO 实例析构后，线程无法执行
}

pid_t Thread::getId() const {
    return m_id;
}

const std::string&
Thread::getName() const {
    return m_name;
}

void Thread::setName(const std::string& name) {
    m_name = name;
}

void Thread::join() {
    if (m_thread) {
        int result = pthread_join(m_thread, nullptr);
        if (result) {
            LOG_FMT_ERROR(
                system_logger,
                "pthread_join() 执行失败，错误码 = %d",
                result);
            throw std::system_error();
        }
        m_thread = 0;
    }
}

void* Thread::Run(void* arg) {
    Thread* thread = (Thread*)arg;
    if (!thread->m_callback) {
        LOG_ERROR(
            system_logger,
            "Thread::Run() 异常，Thread 实例已被析构,无法调用 callback");
        throw std::logic_error("Thread 实例已被析构,无法调用 callback");
    }
    // 赋值将当前线程的局部变量 t_thread
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = GetThreadID();
    // 设置 pthread 线程的线程名
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());
    std::function<void()> callback;
    callback.swap(thread->m_callback);
    callback();
    return 0;
}
}
