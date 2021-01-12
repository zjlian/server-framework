#include <sys/stat.h>
#include <sys/types.h>
#include "fd_manager.h"
#include "hook.h"

namespace zjl
{

FileDescriptor::FileDescriptor(int fd)
    : m_is_init(false),
      m_is_socket(false),
      m_system_non_block(false),
      m_user_non_block(false),
      m_is_closed(false),
      m_fd(fd),
      m_recv_timeout(-1),
      m_send_timeout(-1),
      m_iom(nullptr)
{
    init();
}

FileDescriptor::~FileDescriptor()
{

}

bool  FileDescriptor::init()
{
    if (m_is_init) 
    {
        return true;
    }
    m_recv_timeout = -1;
    m_send_timeout = -1;
    struct stat fd_stat;
    if (fstat(m_fd, &fd_stat) == -1)
    {
        m_is_init = false;
        m_is_socket = false;
    }
    else
    {
        m_is_init = true;
        m_is_socket = S_ISSOCK(fd_stat.st_mode);
    }

    if (m_is_socket)
    {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        // 强制 socket 文件描述符为非阻塞模式
        if (!(flags & O_NONBLOCK))
        {
             fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_system_non_block = true;
    }
    else
    {
        m_system_non_block = false;
    }
    m_user_non_block = false;
    m_is_closed = false;
    return m_is_init;
}

bool FileDescriptor::close()
{
    return false;
}

void FileDescriptor::setTimeout(int type, uint64_t v)
{
    if (type == SO_RCVTIMEO)
    {
        m_recv_timeout = v;
    }
    else
    {
        m_send_timeout = v;
    }
}

uint64_t FileDescriptor::getTimeout(int type)
{
    if (type == SO_RCVTIMEO)
    {
        return m_recv_timeout;
    }
    else
    {
        return m_send_timeout;
    }
}

FileDescriptorManagerImpl::FileDescriptorManagerImpl()
{
    m_data.resize(64);
}

FileDescriptor::ptr FileDescriptorManagerImpl::get(int fd, bool auto_create)
{
    ReadScopedLock lock(&m_lock);
    if (m_data.size() <= static_cast<size_t>(fd))
    {
        if (!auto_create)
        {
            return nullptr;
        }
    }
    else
    {
        if (m_data[fd] || !auto_create)
        {
            return m_data[fd];
        }
    }
    lock.unlock();

    assert(m_data.size() > static_cast<size_t>(fd) && "vector FileDescriptorManagerImpl::m_data 越界访问");

    WriteScopedLock lock2(&m_lock);
    FileDescriptor::ptr fdp(new FileDescriptor(fd));
    m_data[fd] = fdp;
    return fdp;
}

void FileDescriptorManagerImpl::remove(int fd)
{
    WriteScopedLock lock(&m_lock);
    if (m_data.size() <= static_cast<size_t>(fd))
    {
        return;
    }
    m_data[fd].reset();
}


} // namespace zjl
