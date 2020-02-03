#ifndef SERVER_FRAMEWORK_UTIL_H
#define SERVER_FRAMEWORK_UTIL_H

#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <cinttypes>
#include <memory>

namespace zjl {

// 获取linux下线程的唯一id
int64_t getThreadID();
// 获取协程id
int64_t getFiberID();

// 简单单例包装类
template<class T>
class Singleton final {
public:
    static T* GetInstance() {
        static T ins;
        return &ins;
    }
private:
    Singleton() = default;
};

template<class T>
class SingletonPtr final {
public:
    static std::shared_ptr<T> GetInstance() {
        static auto ins = std::make_shared<T>();
        return ins;
    }
private:
    SingletonPtr() = default;
};

}
#endif
