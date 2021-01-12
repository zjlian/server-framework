#ifndef SERVER_FRAMEWORK_FD_MANAGER_H
#define SERVER_FRAMEWORK_FD_MANAGER_H

#include <memory>
#include "thread.h"
#include "io_manager.h"
#include "singleton.h"

namespace zjl
{

/**
 * @brief FileDescriptor 文件描述符包装类
*/
class FileDescriptor : public std::enable_shared_from_this<FileDescriptor>
{
public:
    using ptr = std::shared_ptr<FileDescriptor>;

    FileDescriptor(int fd);
    ~FileDescriptor();

    bool init();
    bool isInit() const { return m_is_init; };
    bool isSocket() const { return m_is_socket; };
    bool isClosed() const { return m_is_closed; };
    bool close();

    void setUserNonBlock(bool v) { m_user_non_block = v; }
    bool getUserNonBlock() const { return m_user_non_block; }
    
    void setSyetemNonBlock(bool v) { m_system_non_block = v; }
    bool getSystemNonBlock() const { return m_system_non_block; }

    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);

private:
    bool m_is_init;
    bool m_is_socket;
    bool m_system_non_block;
    bool m_user_non_block;
    bool m_is_closed;

    int m_fd;

    uint64_t m_recv_timeout;
    uint64_t m_send_timeout;

    zjl::IOManager* m_iom;
};

/**
 * @brief FileDescriptorManagerImpl 文件描述符管理类
*/
class FileDescriptorManagerImpl
{
public:
    using RWLockType = RWLock;

    FileDescriptorManagerImpl();

    /**
     * @brief 获取文件描述符 fd 对应的包装对象，若指定参数 auto_create 为 true, 当该包装对象不在管理类中时，自动创建一个新的包装对象
     * @return 返回指定的文件描述符的包装对象；当指定的文件描述符不存在时，返回 nullptr，如果指定 auto_create 为 true，则为这个文件描述符创建新的包装对象并返回。
    */
    FileDescriptor::ptr get(int fd, bool auto_create = false);

    /**
     * @brief 将一个文件描述符从管理类中删除
    */
   void remove(int fd);
    
private:
    RWLockType m_lock{};
    std::vector<FileDescriptor::ptr> m_data;
};

using FileDescriptorManager = SingletonPtr<FileDescriptorManagerImpl>;

} // namespace zjl

#endif // SERVER_FRAMEWORK_FD_MANAGER
