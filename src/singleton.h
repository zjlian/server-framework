#ifndef SERVER_FRAMEWORK_SINGLETON_H
#define SERVER_FRAMEWORK_SINGLETON_H

#include <memory>

namespace zjl
{

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
