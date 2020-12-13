#include "io_manager.h"
#include "exception.h"
#include "log.h"
#include <array>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

namespace zjl
{

static Logger::ptr system_logger = GET_LOGGER("system");

/**
 * ===================================================
 * IOManager 类的实现
 * ===================================================
*/

IOManager* IOManager::GetThis()
{
    /**
     * NOTE: IOManager 继承于 Scheduler，
     * 直接使用基类实现的 GetThis() 就可以了，将基类指针的类型转换成派生类的
     * */
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

IOManager::IOManager(size_t thread_size, bool use_caller, std::string name)
    : Scheduler(thread_size, use_caller, std::move(name))
{
    LOG_DEBUG(system_logger, "调用 IOManager::IOManager()");
    // 创建 epoll
    m_epoll_fd = ::epoll_create(0xffff);
    if (m_epoll_fd == -1)
    {
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    // 创建管道，并加入 epoll 监听
    if (::pipe(m_tickle_fds) == -1)
    {
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    // 创建管道可读事件监听
    epoll_event event{};
    event.data.fd = m_tickle_fds[0];
    // 监听可读事件 与 开启边缘触发
    event.events = EPOLLIN | EPOLLET;
    // 将管道读取端设置为非阻塞模式
    if (::fcntl(m_tickle_fds[0], F_SETFL, O_NONBLOCK))
    {
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_tickle_fds[0], &event) == -1)
    {
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    contextListResize(64);
    // 启动调度器
    start();
}

IOManager::~IOManager()
{
    // FIXME: 调用了虚函数
    stop();
    // 关闭打开的文件标识符
    close(m_epoll_fd);
    close(m_tickle_fds[0]);
    close(m_tickle_fds[1]);
    // 释放 m_fd_context_list 的指针
    //    for (auto item : m_fd_context_list)
    //    {
    //        delete item;
    //    }
}

void IOManager::contextListResize(size_t size)
{
    m_fd_context_list.resize(size);
    for (size_t i = 0; i < m_fd_context_list.size(); i++)
    {
        if (!m_fd_context_list[i])
        {
            m_fd_context_list[i] = std::make_unique<FDContext>();
            m_fd_context_list[i]->m_fd = i;
        }
    }
}

int IOManager::addEventListener(int fd, FDEventType event, std::function<void()> callback)
{
    /**
     * NOTE:
     *  主要工作流程: 首先从 fd 对象池中取出对应的对象指针，如果不存在就扩容对象池，
     *  第二步检查 fd 对象是否存在相同的事件，
     *  第三步创建 epoll 事件对象，并注册事件,
     *  最后更新 fd 对象的事件处理器
     * */
    FDContext* fd_ctx = nullptr;
    ReadScopedLock lock(&m_lock);
    // 从 m_fd_context_list 中拿对象
    if (m_fd_context_list.size() > static_cast<size_t>(fd))
    {
        fd_ctx = m_fd_context_list[fd].get();
        lock.unlock();
    }
    else
    {
        lock.unlock();
        WriteScopedLock lock2(&m_lock);
        contextListResize(m_fd_context_list.size() * 2);
        fd_ctx = m_fd_context_list[fd].get();
    }
    ScopedLock lock3(&fd_ctx->m_mutex);
    // 检查要监听的事件是否已经存在
    if (fd_ctx->m_events & event)
    {
        LOG_FMT_ERROR(
            system_logger,
            "IOManager::addEventListener 重复添加相同的事件 fd = %d, event = %d, fd_ctx.event = %d",
            fd, event, fd_ctx->m_events);
        assert(!(fd_ctx->m_events & event));
    }
    /*
     * 如果这个 fd context 的 m_events 是空的，说明这个 fd 还没在 epoll 上注册，
     * 使用 EPOLL_CTL_ADD 注册新事件，
     * 否则，使用 EPOLL_CTL_MOD，更改 fd 监听的事件
     **/
    int op = fd_ctx->m_events == FDEventType::NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    // 创建事件
    epoll_event epevent{};
    epevent.events = EPOLLET | fd_ctx->m_events | event;
    /**
     * FIXME: 感觉这是个不太好的做法。 fd_ctx 指向的对象由 unique_ptr 管理，
     *        这相当于交出了所有权，但暂时想不出解决办法。
     * */
    epevent.data.ptr = fd_ctx;
    // 给 fd 注册事件监听
    if (::epoll_ctl(m_epoll_fd, op, fd, &epevent) == -1)
    {
        LOG_FMT_ERROR(
            system_logger,
            "epoll_ctl 调用失败，epfd = %d",
            m_epoll_fd);
        THROW_EXCEPTION_WHIT_ERRNO;
    }
//    LOG_FMT_DEBUG(system_logger, "epoll_ctl %s 注册事件 %ul : %s",
//                  op == 1 ? "新增" : "修改",
//                  epevent.events,
//                  strerror(errno));
    ++m_pending_event_count;
    fd_ctx->m_events = static_cast<FDEventType>(fd_ctx->m_events | event);
    FDContext::EventHandler& event_handler = fd_ctx->getEventHandler(event);
    // 确保没有给这个 fd 没有重复添加事件监听
    assert(event_handler.m_scheduler == nullptr &&
           !event_handler.m_fiber &&
           !event_handler.m_callback);
//    event_handler.m_scheduler = Scheduler::GetThis();
    event_handler.m_scheduler = this;
//    LOG_FMT_ERROR(system_logger, "调度器地址: %p", Scheduler::GetThis());
    if (callback)
    {
        event_handler.m_callback.swap(callback);
    }
    else
    {
        event_handler.m_fiber = Fiber::GetThis();
    }
    return 0;
}

bool IOManager::removeEventListener(int fd, FDEventType event)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (m_fd_context_list.size() <= static_cast<size_t>(fd))
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd].get();
    }
    ScopedLock lock2(&(fd_ctx->m_mutex));
    if (!(fd_ctx->m_events & event))
    { // 要移除的事件不存在
        return false;
    }
    // 从 epoll 上移除该事件的监听
    auto new_event = static_cast<FDEventType>(fd_ctx->m_events & ~event);
    // 如果 new_event 为 0, 从 epoll 中移除对该 fd 的监听，否则仅修改监听事件
    int op = new_event == FDEventType::NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    epoll_event epevent{};
    epevent.events = EPOLLET | new_event;
    epevent.data.ptr = fd_ctx;
    if (::epoll_ctl(m_epoll_fd, op, fd, &epevent) == -1)
    {
        LOG_FMT_ERROR(
            system_logger,
            "epoll_ctl 调用失败，epfd = %d",
            m_epoll_fd);
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    fd_ctx->m_events = new_event;
    auto& event_handler = fd_ctx->getEventHandler(event);
    fd_ctx->resetHandler(event_handler);
    --m_pending_event_count;
    return true;
}

bool IOManager::cancelEventListener(int fd, FDEventType event)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (m_fd_context_list.size() <= static_cast<size_t>(fd))
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd].get();
    }
    ScopedLock lock2(&(fd_ctx->m_mutex));
    if (!(fd_ctx->m_events & event))
    { // 要移除的事件不存在
        return false;
    }
    // 从 epoll 上移除该事件的监听
    auto new_event = static_cast<FDEventType>(fd_ctx->m_events & ~event);
    // 如果 new_event 为 0, 从 epoll 中移除对该 fd 的监听，否则仅修改监听事件
    int op = new_event == FDEventType::NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    epoll_event epevent{};
    epevent.events = EPOLLET | new_event;
    epevent.data.ptr = fd_ctx;
    if (::epoll_ctl(m_epoll_fd, op, fd, &epevent) == -1)
    {
        LOG_FMT_ERROR(
            system_logger,
            "epoll_ctl 调用失败，epfd = %d",
            m_epoll_fd);
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    fd_ctx->m_events = new_event;
    fd_ctx->triggerEvent(event);
    --m_pending_event_count;
    return true;
}

bool IOManager::cancelAll(int fd)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (m_fd_context_list.size() <= static_cast<size_t>(fd))
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd].get();
    }
    ScopedLock lock2(&(fd_ctx->m_mutex));
    if (!(fd_ctx->m_events))
    { // 不存在监听的事件
        return false;
    }
    // 从 epoll 上移除对该 fd 的监听
    //    epoll_event epevent{};
    if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        LOG_FMT_ERROR(
            system_logger,
            "epoll_ctl 调用失败，epfd = %d",
            m_epoll_fd);
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    if (fd_ctx->m_events & FDEventType::READ)
    {
        fd_ctx->triggerEvent(FDEventType::READ);
        --m_pending_event_count;
    }
    if (fd_ctx->m_events & FDEventType::WRITE)
    {
        fd_ctx->triggerEvent(FDEventType::WRITE);
        --m_pending_event_count;
    }
    fd_ctx->m_events = FDEventType::NONE;
    return true;
}

void IOManager::tickle()
{
    if (hasIdleThread())
    {
        return;
    }
    if (write(m_tickle_fds[1], "T", 1) == -1)
    {
        throw zjl::SystemError("向子线程发送消息失败");
    }
}

bool IOManager::isStop()
{
    uint64_t timeout;
    return isStop(timeout);
}

bool IOManager::isStop(uint64_t& timeout)
{
    timeout = getNextTimer();
    return timeout == ~0ull &&
        m_pending_event_count == 0 &&
        Scheduler::isStop();
}

void IOManager::onIdle()
{
    LOG_DEBUG(system_logger, "调用 IOManager::onIdle()");
    auto event_list = std::make_unique<epoll_event[]>(64);

    while (true)
    {
        uint64_t next_timeout = 0;
        if (isStop(next_timeout))
        {
            // 没有等待执行的定时器
            if (next_timeout == ~0ull)
            {
                LOG_FMT_DEBUG(
                    system_logger, "I/O 调度器 %s 已停止执行", m_name.c_str());
                break;
            }
        }

        int result = 0;
        while (true)
        {
            static const int MAX_TIMEOUT = 1000;
            if (next_timeout != ~0ull)
            {
                next_timeout = static_cast<int>(next_timeout) > MAX_TIMEOUT 
                    ? MAX_TIMEOUT : next_timeout;
            }
            else
            {
                next_timeout = MAX_TIMEOUT;
            }
            // 阻塞等待 epoll 返回结果
            result = ::epoll_wait(m_epoll_fd, event_list.get(), 64, static_cast<int>(next_timeout));

            if (result < 0 /*&& errno == EINTR*/)
            {
                // TODO 处理 epoll_wait 异常
            }
            if (result >= 0)
            {
                break;
            }
        }
        
        // 处理定时器
        std::vector<std::function<void()>> fns;
        listExpiredCallback(fns);
        if (!fns.empty())
        {
            schedule(fns.begin(), fns.end());
        }

        // 遍历 event_list 处理被触发事件的 fd
        for (int i = 0; i < result; i++)
        {
            epoll_event& ev = event_list[i];
            // 接收到来自主线程的消息
            if (ev.data.fd == m_tickle_fds[0])
            {
                char dummy;
                // 将来自主线程的数据读取干净
                while (true)
                {
                    int status = read(ev.data.fd, &dummy, 1);
                    if (status == 0 || status == -1)
                        break;
                }
                continue;
            }
            // 处理非主线程的消息
            auto fd_ctx = static_cast<FDContext*>(ev.data.ptr);
            ScopedLock lock(&fd_ctx->m_mutex);
            // 该事件的 fd 出现错误或者已经失效
            if (ev.events & (EPOLLERR | EPOLLHUP))
            {
                ev.events |= EPOLLIN | EPOLLOUT;
            }
            uint32_t real_events = FDEventType::NONE;
            if (ev.events & EPOLLIN)
            {
                real_events |= FDEventType::READ;
            }
            if (ev.events & EPOLLOUT)
            {
                real_events |= FDEventType::WRITE;
            }
            // fd_ctx 中指定监听的事件都已经被触发并处理
            if ((fd_ctx->m_events & real_events) == FDEventType::NONE)
            {
                continue;
            }
            // 从 epoll 中移除这个 fd 的被触发的事件的监听
            uint32_t left_events = (fd_ctx->m_events & ~real_events);
            int op = left_events == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
            ev.events = EPOLLET | left_events;
            int rt = ::epoll_ctl(m_epoll_fd, op, fd_ctx->m_fd, &ev);
            // epoll 事件修改失败，打印一条 ERROR 日志，不做任何处理
            if (rt == -1)
            {
                LOG_FMT_ERROR(
                    system_logger,
                    "epoll_ctl(%d, %d, %d, %ul) : return %d, errno = %d, %s",
                    m_epoll_fd, op, fd_ctx->m_fd, ev.events, rt, errno, strerror(errno));
            }
            // 触发该 fd 对应的事件的处理器
            if (real_events & FDEventType::READ)
            {
                fd_ctx->triggerEvent(FDEventType::READ);
                --m_pending_event_count;
            }
            if (real_events & FDEventType::WRITE)
            {
                fd_ctx->triggerEvent(FDEventType::WRITE);
                --m_pending_event_count;
            }
        }
        // 让出当前线程的执行权，给调度器执行排队等待的协程
        Fiber::YieldToHold();
    }
}

void IOManager::onTimerInsertedAtFirst()
{
    tickle();
}

/**
 * ===================================================
 * IOManager::FDContext 类的实现
 * ===================================================
*/

FDContext::EventHandler&
FDContext::getEventHandler(FDEventType type)
{
    switch (type)
    {
        case FDEventType::READ:
            return m_read_handler;
        case FDEventType::WRITE:
            return m_write_handler;
        default:
            assert(0);
    }
}

void FDContext::resetHandler(FDContext::EventHandler& handler)
{
    handler.m_fiber.reset();
    handler.m_callback = nullptr;
    handler.m_scheduler = nullptr;
}

void FDContext::triggerEvent(FDEventType type)
{
    /**
     * NOTE: 调用调度器的 schedule 方法时，传参使用了 move 语义，等于说调用了本 triggerEvent 方法后，
     *      会把处理对应事件的协程或函数对象的引用从本 FDContext 对象中移除。
     * */
    assert(m_events & type);
    m_events = static_cast<FDEventType>(m_events & ~type);
    auto& handler = getEventHandler(type);
    assert(handler.m_scheduler);
    // 安排！
    if (handler.m_fiber)
    {
        handler.m_scheduler->schedule(std::move(handler.m_fiber));
    }
    else if (handler.m_callback)
    {
        handler.m_scheduler->schedule(std::move(handler.m_callback));
    }
    handler.m_scheduler = nullptr;
}

} // namespace zjl
