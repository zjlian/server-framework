#include <dlfcn.h>
#include "hook.h"
#include "io_manager.h"
#include "log.h"
#include "fd_manager.h"
#include "config.h"

namespace zjl
{

static thread_local bool t_hook_enabled = false;

static Logger::ptr system_logger = GET_LOGGER("system");

static zjl::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    zjl::Config::Lookup("tcp.connect.timeout", 5000);

#define DEAL_FUNC(DO) \
    DO(sleep) \
    DO(usleep) \
    DO(nanosleep) \
    DO(socket) \
    DO(connect) \
    DO(accept) \
    DO(recv) \
    DO(recvfrom) \
    DO(recvmsg) \
    DO(send) \
    DO(sendto) \
    DO(sendmsg) \
    DO(getsockopt) \
    DO(setsockopt) \
    DO(read) \
    DO(write) \
    DO(close) \
    DO(readv) \
    DO(writev) \
    DO(fcntl) \
    DO(ioctl)

void hook_init()
{
    static bool is_inited = false;
    if (is_inited)
        return;
    
#define TRY_LOAD_HOOK_FUNC(name) name##_f = (name##_func)dlsym(RTLD_NEXT, #name);
    // 利用宏获取指定的系统 api 的函数指针
    DEAL_FUNC(TRY_LOAD_HOOK_FUNC)
#undef TRY_LOAD_HOOK_FUNC
}

static uint64_t s_connect_timeout = -1;
struct _HookIniter
{
    _HookIniter()
    { 
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();
        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
            LOG_FMT_INFO(system_logger, "tcp connect timeout change from %d to %d", old_value, new_value);
            s_connect_timeout = new_value;
        });                               
    }
};
static _HookIniter s_hook_initer;

bool isHookEnabled()
{
    return t_hook_enabled;
}

void setHookEnable(bool flag)
{
    t_hook_enabled = flag; 
}

} // namespace zjl

struct TimerInfo
{
    int cancelled = 0;
};

template<typename OriginFunc, typename ...Args>
static ssize_t doIO(int fd, OriginFunc func, const char* hook_func_name, 
                    uint32_t event, int fd_timeout_type, Args&& ...args)
{
    if (!zjl::t_hook_enabled)
    {
        return func(fd, std::forward<Args>(args)...);
    }

    // LOG_FMT_DEBUG(zjl::system_logger, "doIO 代理执行系统函数 %s", hook_func_name);

    zjl::FileDescriptor::ptr fdp = zjl::FileDescriptorManager::GetInstance()->get(fd);
    if (!fdp)
    {
        return func(fd, std::forward<Args>(args)...);
    }
    if (fdp->isClosed())
    {
        errno = EBADF;
        return -1;
    }
    if (!fdp->isSocket() || fdp->getUserNonBlock())
    {
        return func(fd, std::forward<Args>(args)...);
    }

    uint64_t timeout = fdp->getTimeout(fd_timeout_type);
    auto timer_info = std::make_shared<TimerInfo>();
RETRY:
    ssize_t n = func(fd, std::forward<Args>(args)...);
    // 出现错误 EINTR，是因为系统 API 在阻塞等待状态下被其他的系统信号中断执行
    // 此处的解决办法就是重新调用这次系统 API
    while(n == -1 && errno == EINTR)
    {        
        n = func(fd, std::forward<Args>(args)...);
    }
    // 出现错误 EAGAIN，是因为长时间未读到数据或者无法写入数据，直接把这个 fd 丢到 IOManager 里监听对应事件，触发后返回本执行上下文 
    if (n == -1 && errno == EAGAIN)
    {
        LOG_FMT_DEBUG(zjl::system_logger, "doIO(%s): 开始异步等待", hook_func_name);

        auto iom = zjl::IOManager::GetThis();
        zjl::Timer::ptr timer;
        std::weak_ptr<TimerInfo> timer_info_wp(timer_info);
        // 如果设置了超时时间，在指定时间后取消掉该 fd 的事件监听
        if (timeout != static_cast<uint64_t>(-1))
        {
            timer = iom->addConditionTimer(
                timeout, 
                [timer_info_wp, fd, iom, event](){
                    auto t = timer_info_wp.lock();
                    if (!t || t->cancelled)
                    {
                        return;
                    }
                    t->cancelled = ETIMEDOUT;
                    iom->cancelEventListener(fd, static_cast<zjl::FDEventType>(event));
                }, 
                timer_info_wp);
        }
        int c = 0;
        uint64_t now = 0;

        int rt = iom->addEventListener(fd, static_cast<zjl::FDEventType>(event));
        if (rt == -1)
        {
            LOG_FMT_ERROR(zjl::system_logger, "%s addEventListener(%d, %u)", hook_func_name, fd, event);
            if (timer)
            {
                timer->cancel();
            }
            return -1;
        }
        zjl::Fiber::YieldToHold();

        if (timer)
        {
            timer->cancel();
        }
        if (timer_info->cancelled)
        {
            errno = timer_info->cancelled;
            return -1;
        }
        goto RETRY;
    }
    return n;
}

extern "C" 
{
#define DEF_FUNC_NAME(name) name##_func name##_f = nullptr;
    // 定义系统 api 的函数指针的变量
    DEAL_FUNC(DEF_FUNC_NAME)
#undef DEF_FUNC_NAME

/**
 * @brief hook 处理后的 sleep，利用 IOManager 的定时器功能实现，创建一个延迟时间为 seconds 的定时器，用于将本协程重新加入调度，随后便换出当前协程 
*/
unsigned int sleep(unsigned int seconds)
{
    if (!zjl::t_hook_enabled)
    {
        return sleep_f(seconds);
    }
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(seconds * 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

/**
 * @brief hook 处理后的 usleep
*/
int usleep(useconds_t usec)
{
    if (!zjl::t_hook_enabled)
    {
        return usleep_f(usec);
    }
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(usec / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!zjl::t_hook_enabled)
    {
        return nanosleep_f(req, rem);
    }
    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(timeout_ms, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

//////// sys/socket.h

int socket(int domain, int type, int protocol)
{
    if (!zjl::t_hook_enabled)
    {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1)
    {
        return fd;
    }
    zjl::FileDescriptorManager::GetInstance()->get(fd, true);
    return fd;
}

int connectWithTimeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms)
{
    if (!zjl::t_hook_enabled)
    {
        return connect_f(sockfd, addr, addrlen);
    }
    auto fdp = zjl::FileDescriptorManager::GetInstance()->get(sockfd);
    if (!fdp || fdp->isClosed())
    {
        errno = EBADF;
        return -1;
    }
    if (!fdp->isSocket())
    {
        return connect_f(sockfd, addr, addrlen);
    }
    if (fdp->getUserNonBlock())
    {
        return connect_f(sockfd, addr, addrlen);
    }
    int n = connect_f(sockfd, addr, addrlen);
    if (n == 0)
    {
        return 0;
    }
    else if (n != -1 || errno != EINPROGRESS)
    {
        return n;
    }
    /**
     * 调用 connect，非阻塞形式下会返回-1，但是 errno 被设为 EINPROGRESS，表明 connect 仍旧在进行还没有完成。
     * 下一步就需要为其添加 write 事件监听，当连接成功后会触发该事件。
    */
    auto iom = zjl::IOManager::GetThis();
    zjl::Timer::ptr timer;
    auto timer_info = std::make_shared<TimerInfo>();
    std::weak_ptr<TimerInfo> weak_timer_info(timer_info);

    if (timeout_ms != static_cast<uint64_t>(-1))
    {
        timer = iom->addConditionTimer(
            timeout_ms, 
            [weak_timer_info, sockfd, iom](){
                auto t = weak_timer_info.lock();
                if (!t || t->cancelled)
                {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEventListener(sockfd, zjl::FDEventType::WRITE);
            }, 
            weak_timer_info);
    }

    int rt = iom->addEventListener(sockfd, zjl::FDEventType::WRITE);
    if (rt == 0)
    {
        zjl::Fiber::YieldToHold();
        if (timer)
        {
            timer->cancel();
        }
        if (timer_info->cancelled)
        {
            errno = timer_info->cancelled;
            return -1;
        }
    }
    if (rt == -1)
    {
        if (timer)
        {
            timer->cancel();
        }
        LOG_FMT_ERROR(zjl::system_logger, "connectWithTimeout addEventListener(%d, write) error", sockfd);
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
    {
        return -1;
    }
    
    if (!error)
    {
        return 0;
    }
    else 
    {
        errno = error;
        return -1;    
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connectWithTimeout(sockfd, addr, addrlen, zjl::s_connect_timeout);
}   

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd = doIO(sockfd, accept_f, "accept", zjl::FDEventType::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0)
    {
        zjl::FileDescriptorManager::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count)
{
    return doIO(fd, read_f, "read", zjl::FDEventType::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return doIO(fd, readv_f, "readv", zjl::FDEventType::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    return doIO(sockfd, recv_f, "recv", zjl::FDEventType::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, 
    struct sockaddr* src_addr, socklen_t* addrlen)
{
    return doIO(sockfd, recvfrom_f, "recvfrom", zjl::FDEventType::READ, SO_RCVTIMEO, 
        buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    return doIO(sockfd, recvmsg_f, "recvfrom", zjl::FDEventType::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return doIO(fd, write_f, "write", zjl::FDEventType::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    return doIO(fd, writev_f, "writev_f", zjl::FDEventType::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    return doIO(sockfd, send_f, "send", zjl::FDEventType::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return doIO(sockfd, sendto_f, "sendto", zjl::FDEventType::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    return doIO(sockfd, sendmsg_f, "sendmsg", zjl::FDEventType::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd)
{
    if (!zjl::t_hook_enabled)
    {
        return close_f(fd);
    }
    zjl::FileDescriptor::ptr fdp = zjl::FileDescriptorManager::GetInstance()->get(fd);
    if (fdp)
    {
        auto iom = zjl::IOManager::GetThis();
        if (iom)
        {
            iom->cancelAll(fd);
        }
        zjl::FileDescriptorManager::GetInstance()->remove(fd);
    }
    return close_f(fd);
} 

int fcntl(int fd, int cmd, ... /* arg */ )
{
    if (!zjl::t_hook_enabled)
    {
        // return fcntl_f(fd, cmd, );
    }
    va_list va;
    va_start(va, cmd);
    switch (cmd)
    {
        case F_SETFL:
        {
            int arg = va_arg(va, int);
            va_end(va);
            auto fdp = zjl::FileDescriptorManager::GetInstance()->get(fd);
            if (!fdp || fdp->isClosed() || !fdp->isSocket())
            {
                return fcntl_f(fd, cmd, arg);
            }
            fdp->setUserNonBlock(arg & O_NONBLOCK);
            // 根据这个 fd 的包装对象是否设置了系统层面的非阻塞，
            if (fdp->getSystemNonBlock())
            {
                arg |= O_NONBLOCK;
            }
            else
            {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
            break;

        case F_GETFL:
        {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            auto fdp = zjl::FileDescriptorManager::GetInstance()->get(fd);
            if (!fdp || fdp->isClosed() || !fdp->isSocket())
            {
                return arg;
            }
            if (fdp->getUserNonBlock())
            {
                return arg | O_NONBLOCK;                
            }
            else
            {
                return arg & ~O_NONBLOCK;
            }
        }
            break;

        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;

        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
            break;

        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            auto arg = va_arg(va, flock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;

        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            auto arg = va_arg(va, f_owner_ex*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;

        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);
    
    if (FIONBIO ==request)
    {
        bool user_nonblock = !!*(int*)arg;
        auto fdp = zjl::FileDescriptorManager::GetInstance()->get(fd);
        if (!fdp || fdp->isClosed() || !fdp->isSocket())
        {
            return ioctl_f(fd, request, arg);
        }
        fdp->setUserNonBlock(user_nonblock);
    }
    return ioctl(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    if (!zjl::t_hook_enabled)
    {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    //
    if (level == SOL_SOCKET)
    {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
        {
            auto fdp = zjl::FileDescriptorManager::GetInstance()->get(sockfd);
            if (fdp)
            {
                const timeval* v = static_cast<const timeval*>(optval);
                fdp->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

} // extern "C"
