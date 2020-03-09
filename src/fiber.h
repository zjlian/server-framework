#ifndef SERVER_FRAMEWORK_FIBER_H
#define SERVER_FRAMEWORK_FIBER_H

#include "config.h"
#include "thread.h"
#include <atomic>
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

    // 协程状态，用于调度
    enum State
    {
        INIT,     // 初始化
        READY,    // 预备
        HOLD,     // 挂起
        EXEC,     // 执行
        TERM,     // 结束
        EXCEPTION // 异常
    };

public:
    /**
     * @brief 创建新协程
     * @param callback 协程执行函数
     * @param 协程栈大小，如果传 0，使用配置项 "fiber.stack_size" 定义的值
     * */
    Fiber(FiberFunc callback, size_t stack_size = 0);
    ~Fiber();
    // 更换协程执行函数
    void reset(FiberFunc callback);
    // 换入协程
    void swapIn();
    // 挂起协程
    void swapOut();
    // 获取协程 id
    uint64_t getID() const { return m_id; }

private:
    // 用于创建 master fiber
    Fiber();

public:
    // 获取当前 fiber 的指针指针
    static Fiber::ptr GetThis();
    // 设置当前 fiber
    static void SetThis(Fiber* fiber);
    // 挂起当前协程，转换为 READY 状态，等待下一次调度
    static void YieldToReady();
    // 挂起当前协程，转换为 HOLD 状态，等待下一次调度
    static void YieldToHold();
    // 获取存在的协程数量
    static uint64_t TotalFiber();
    // 获取协程 id
    static uint64_t GetFiberID();
    // 协程入口函数
    static void MainFunc();

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
    void* m_stack;
    // 协程执行函数
    FiberFunc m_callback;
};

namespace FiberInfo
{

// 最后一个协程的 id
static std::atomic_uint64_t s_fiber_id{0};
// 存在的协程数量
static std::atomic_uint64_t s_fiber_count{0};

// 当前协程
static thread_local Fiber* t_fiber = nullptr;
// 主协程
static thread_local Fiber::ptr t_master_fiber{};

// 协程栈大小配置项
static ConfigVar<uint64_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint64_t>("fiber.stack_size", 1024 * 1024);
} // namespace FiberInfo

} // namespace zjl

#endif
