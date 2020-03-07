#ifndef SERVER_FRAMEWORK_FIBER_H
#define SERVER_FRAMEWORK_FIBER_H

#include "thread.h"
#include <functional>
#include <memory>
#include <ucontext.h>

namespace zjl
{

/**
 * @brief 协程类
*/
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
    using ptr = std::shared_ptr<Fiber>;
    using uptr = std::unique_ptr<Fiber>;
    using FiberFunc = std::function<void()>;

    // 协程状态
    enum State
    {
        INIT,  // 初始化
        HOLD,  // 挂起
        EXEC,  // 执行
        TERM,  //
        READY, // 预备
    };

public:
    Fiber(FiberFunc callback, size_t stack_size);
    ~Fiber();

    void reset(FiberFunc callback);
    // 换入协程，开始执行
    void swapIn();
    // 挂起协程
    void swapOut();

public:
    // 获取当前协程对象
    static Fiber::ptr GetThis();
    // 挂起当前协程，转换为 READY 状态
    static void YieldToReady();
    // 挂起当前协程，转换为 HOLD 状态
    static void YieldToHold();
    // 获取存在的协程数量
    static uint64_t TotalFiber();

private:
    // 协程 id
    uint64_t m_id;
    // 协程栈大小
    uint64_t m_stack_size;
    // 协程状态
    State m_state;
    // 协程上下文
    ucontext_t m_ctx;
    // 协程栈空间指针
    void* m_stakc;
    // 协程执行函数
    FiberFunc m_callback;
};
} // namespace zjl

#endif
