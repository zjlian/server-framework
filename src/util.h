#ifndef SERVER_FRAMEWORK_UTIL_H
#define SERVER_FRAMEWORK_UTIL_H

#include <cinttypes>
#include <memory>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace zjl
{

// 获取linux下线程的唯一id
int GetThreadID();
// 获取协程id
int GetFiberID();

/**
 * @brief 简单的单例包装类
 * 调用 Singleton::GetInstance 返回被包装类型的原生指针
*/
template <class T>
class Singleton final
{
public:
    static T* GetInstance()
    {
        static T ins;
        return &ins;
    }

private:
    Singleton() = default;
};

/**
 * @brief 简单的单例包装类
 * 调用 Singleton::GetInstance 返回被包装类型的 std::shared_ptr 智能指针
*/
template <class T>
class SingletonPtr final
{
public:
    static std::shared_ptr<T> GetInstance()
    {
        static auto ins = std::make_shared<T>();
        return ins;
    }

private:
    SingletonPtr() = default;
};

} // namespace zjl
#endif
