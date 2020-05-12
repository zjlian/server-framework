#ifndef SERVER_FRAMEWORK_IOMANAGER_H
#define SERVER_FRAMEWORK_IOMANAGER_H

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
    enum EventType
    {
        NONE = 0x0,
        READ = 0x1,
        WRITE = 0x2
    };

private: // 内部类
    // 记录与 fd 相关的信息
    struct FDContext
    {
        using MutexType = zjl::Mutex;
        struct EventHandler
        {
            Scheduler* m_scheduler;      // 指定处理该事件的调度器
            Fiber::ptr m_fiber;          // 要跑的协程
            Fiber::FiberFunc m_callback; // 要跑的函数，fiber 和 callback 只需要存在一个
        };
        // 获取指定事件的处理器
        EventHandler& getEventHandler(EventType type);
        // 清除指定的事件处理器
        void resetHandler(EventHandler& handler);
        // 触发事件
        void triggerEvent(EventHandler& handler);

        MutexType m_mutex;
        EventHandler m_read_handler;  // 处理读事件的
        EventHandler m_write_handler; // 处理写事件的
        int m_fd;                     // 要监听的文件描述符
        EventType m_events = EventType::NONE;
    };

public: // 实例方法
    explicit IOManager(size_t thread_size, bool use_caller = true, std::string name = "");
    ~IOManager() override;

    // 给指定的 fd 增加事件监听
    int addEventListener(int fd, EventType event, std::function<void()> callback);
    // 给指定的 fd 移除指定的事件监听
    bool removeEventListener(int fd, EventType event);
    // 立即触发指定 fd 的指定的事件，然后移除该事件
    bool cancelEventListener(int fd, EventType event);
    // 立即触发指定 fd 的所有事件，然后移除所有的事件
    bool cancelAll(int fd);

public: // 类方法
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool onStop() override;
    bool onIdle() override;
    void contextListResize(size_t size);

private: // 私有成员
    LockType m_lock{};
    int m_epoll_fd = 0;                          // epoll 文件标识符
    int m_tickle_fds[2]{0};                      // 主线程给子线程发消息用的管道
    std::atomic_size_t m_pending_event_count{0}; // 等待执行的事件的数量
    std::vector<FDContext*> m_fd_context_list{}; // FDContext 的对象池，下标对应 fd id
};
} // namespace zjl

#endif //SERVER_FRAMEWORK_IOMANAGER_H