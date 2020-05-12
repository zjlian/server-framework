#include "IOManager.h"
#include "exception.h"
#include "log.h"
#include <cstring>
#include <fcntl.h>
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
    if (!isStop())
    {
        stop();
    }
    // 关闭打开的文件标识符
    close(m_epoll_fd);
    close(m_tickle_fds[0]);
    close(m_tickle_fds[1]);
    // 释放 m_fd_context_list 的指针
    for (auto item : m_fd_context_list)
    {
        delete item;
    }
}

void IOManager::contextListResize(size_t size)
{
    m_fd_context_list.resize(size);
    for (size_t i = 0; i < m_fd_context_list.size(); i++)
    {
        if (!m_fd_context_list[i])
        {
            m_fd_context_list[i] = new FDContext;
            m_fd_context_list[i]->m_fd = i;
        }
    }
}

int IOManager::addEventListener(int fd, IOManager::EventType event, std::function<void()> callback)
{
    /**
     * NOTE:
     *  主要工作流程: 首先从 fd 对象池中取出对应的对象指针，如果不存在就扩容对象池，
     *  第二步检查 fd 对象是否存在相同的事件，存在的话就抛出异常，
     *  第三步创建 epoll 事件对象，并注册事件
     *  最后一步是更新 fd 对象的事件处理器
     * */
    FDContext* fd_ctx = nullptr;
    ReadScopedLock lock(&m_lock);
    // 从 m_fd_context_list 中拿对象
    if (static_cast<int>(m_fd_context_list.size()) > fd)
    {
        fd_ctx = m_fd_context_list[fd];
        lock.unlock();
    }
    else
    {
        lock.unlock();
        WriteScopedLock lock2(&m_lock);
        contextListResize(m_fd_context_list.size() * 2);
        fd_ctx = m_fd_context_list[fd];
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
    int op = fd_ctx->m_events == EventType::NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    // 创建事件
    epoll_event epevent{};
    epevent.events = EPOLLET | fd_ctx->m_events | event;
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
    ++m_pending_event_count;
    fd_ctx->m_events = static_cast<EventType>(fd_ctx->m_events | event);
    FDContext::EventHandler& event_handler = fd_ctx->getEventHandler(event);
    // 确保没有给这个 fd 没有重复添加事件监听
    assert(event_handler.m_scheduler == nullptr &&
           !event_handler.m_fiber &&
           !event_handler.m_callback);
    event_handler.m_scheduler = Scheduler::GetThis();
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

bool IOManager::removeEventListener(int fd, IOManager::EventType event)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (static_cast<int>(m_fd_context_list.size()) <= fd)
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd];
    }
    ScopedLock lock2(&(fd_ctx->m_mutex));
    if (!(fd_ctx->m_events & event))
    { // 要移除的事件不存在
        return false;
    }
    // 从 epoll 上移除该事件的监听
    auto new_event = static_cast<EventType>(fd_ctx->m_events & ~event);
    // 如果 new_event 为 0, 从 epoll 中移除对该 fd 的监听，否则仅修改监听事件
    int op = new_event == EventType::NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
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

bool IOManager::cancelEventListener(int fd, IOManager::EventType event)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (static_cast<int>(m_fd_context_list.size()) <= fd)
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd];
    }
    ScopedLock lock2(&(fd_ctx->m_mutex));
    if (!(fd_ctx->m_events & event))
    { // 要移除的事件不存在
        return false;
    }
    // 从 epoll 上移除该事件的监听
    auto new_event = static_cast<EventType>(fd_ctx->m_events & ~event);
    // 如果 new_event 为 0, 从 epoll 中移除对该 fd 的监听，否则仅修改监听事件
    int op = new_event == EventType::NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
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
    // auto& event_handler = fd_ctx->getEventHandler(event);
    fd_ctx->triggerEvent(event);
    // TODO 是否需要清除对应的处理器？
    // fd_ctx->resetHandler(event_handler);
    --m_pending_event_count;
    return true;
}

bool IOManager::cancelAll(int fd)
{
    FDContext* fd_ctx = nullptr;
    { // 上读锁
        ReadScopedLock lock(&m_lock);
        if (static_cast<int>(m_fd_context_list.size()) <= fd)
        {
            return false;
        }
        // 从 m_fd_context_list 中拿对象
        fd_ctx = m_fd_context_list[fd];
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
    if (fd_ctx->m_events & EventType::READ)
    {
        auto& event_handler = fd_ctx->getEventHandler(EventType::READ);
        fd_ctx->triggerEvent(event_handler);
        --m_pending_event_count;
    }
    if (fd_ctx->m_events & EventType::WRITE)
    {
        auto& event_handler = fd_ctx->getEventHandler(EventType::WRITE);
        fd_ctx->triggerEvent(event_handler);
        --m_pending_event_count;
    }
    fd_ctx->m_events = EventType::NONE;
    return true;
}

void zjl::IOManager::tickle()
{
    Scheduler::tickle();
}

bool zjl::IOManager::onStop()
{
    return Scheduler::onStop();
}

bool zjl::IOManager::onIdle()
{
    return Scheduler::onIdle();
}

/**
 * ===================================================
 * IOManager::FDContext 类的实现
 * ===================================================
*/

IOManager::FDContext::EventHandler&
IOManager::FDContext::getEventHandler(IOManager::EventType type)
{
    switch (type)
    {
        case EventType::READ:
            return m_read_handler;
        case EventType::WRITE:
            return m_write_handler;
        default:
            assert(0);
    }
}

void IOManager::FDContext::resetHandler(IOManager::FDContext::EventHandler& handler)
{
    handler.m_fiber.reset();
    handler.m_callback = nullptr;
    handler.m_scheduler = nullptr;
}

void IOManager::FDContext::triggerEvent(IOManager::FDContext::EventHandler& handler)
{
}

} // namespace zjl