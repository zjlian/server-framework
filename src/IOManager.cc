#include "IOManager.h"
#include "exception.h"
#include "log.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sys/epoll.h>

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
    return nullptr;
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
    // 创建管道
    if (::pipe(m_tickle_fds) == -1)
    {
        THROW_EXCEPTION_WHIT_ERRNO;
    }
    // 创建管道可读事件监听
    struct epoll_event event{};
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
    m_fd_context_list.resize(64);
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
    for (auto fdc : m_fd_context_list)
    {
        delete fdc;
    }
}

int IOManager::addEventListener(int fd, IOManager::EventType event, std::function<void()> callback)
{
    return 0;
}

bool IOManager::removeEventListener(int fd, IOManager::EventType event)
{
    return false;
}

bool IOManager::cancelEventListener(int fd, IOManager::EventType event)
{
    return false;
}

bool IOManager::cancelAll(int fd)
{
    return false;
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

} // namespace zjl