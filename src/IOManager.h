#ifndef SERVER_FRAMEWORK_IO_MANAGER_H
#define SERVER_FRAMEWORK_IO_MANAGER_H

#include "scheduler.h"
#include "thread.h"
#include <atomic>
#include <functional>
#include <memory>

namespace zjl
{

class IOManager : public Scheduler
{
public: // 内部类型
    using ptr = std::shared_ptr<IOManager>;
    using LockType = zjl::RWLock;
    enum class EventType
    {
        NONE,
        READ,
        WRITE
    };

private: // 内部类
    struct FDContext
    {
        using MutexType = zjl::Mutex;
        struct EventHandler
        {
            Scheduler* m_scheduler;      // 指定处理该事件的调度器
            Fiber::ptr m_fiber;          // 要跑的协程
            Fiber::FiberFunc m_callback; // 要跑的函数，fiber 和 callback 只需要存在一个
        };
        MutexType m_mutex;
        EventHandler m_read_handler;  // 处理读事件的
        EventHandler m_write_handler; // 处理写事件的
        int fd;                       // 要监听的文件描述符
        EventType m_event_type = EventType::NONE;
    };

public: // 实例方法
    explicit IOManager(size_t thread_size, bool use_caller = true, std::string name = "");
    ~IOManager() override;

    int addEventListener(int fd, EventType event, std::function<void()> callback);
    bool removeEventListener(int fd, EventType event);
    bool cancelEventListener(int fd, EventType event);
    bool cancelAll(int fd);

public: // 类方法
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool onStop() override;
    bool onIdle() override;

private: // 私有成员
    LockType lock{};
    int m_epoll_fd = 0;                          // epoll 文件标识符
    int m_tickle_fds[2]{0};                      // 主线程给子线程发消息用的管道
    std::atomic_size_t m_pending_event_count{0}; // 等待执行的事件的数量
    std::vector<FDContext*> m_fd_context_list{};
};
} // namespace zjl

#endif //SERVER_FRAMEWORK_IO_MANAGER_H
