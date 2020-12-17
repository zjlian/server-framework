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

class Scheduler;

/**
 * @brief 协程类
*/
class Fiber : public std::enable_shared_from_this<Fiber>, public zjl::noncopyable
{
    friend class Scheduler;
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
    explicit Fiber(FiberFunc callback, size_t stack_size = 0);
//    Fiber(const Fiber& rhs);
    ~Fiber();

    // 更换协程执行函数
    void reset(FiberFunc callback);

    // 换入协程，该方法通常 master fiber 调用
    void swapIn();

    // 挂起协程，该方法通常 master fiber 调用
    void swapOut();

    // 换入协程，将调用时的上下挂起到保存到线程局部变量中
    void call();

    // 挂起协程，保存当前上下文到协程对象中，从线程局部变量恢复执行上下文
    void back();

    /**
     * @brief 换入协程，该方法通常由调度器调用
     * @param ctx 指定存储当前协程上下文信息的指针
     * */
    void swapIn(Fiber::ptr fiber);

    /**
     * @brief 挂起协程，该方法通常由调度器调用
     * @param ctx 指定要恢复的协程
     * */
    void swapOut(Fiber::ptr fiber);

    // 获取协程 id
    uint64_t getID() const { return m_id; }

    // 获取协程状态
    State getState() const { return m_state; }

    // 判断协程是否执行结束
    bool finish() const noexcept;

private:
    // 用于创建 master fiber
    Fiber();

public:
    // 获取当前正在执行的 fiber 的智能指针，如果不存在，则在当前线程上创建 master fiber
    static Fiber::ptr GetThis();
    // 设置当前 fiber
    static void SetThis(Fiber* fiber);
    // 挂起当前协程，转换为 READY 状态，等待下一次调度
    static void YieldToReady();
    // 挂起当前协程，转换为 HOLD 状态，等待下一次调度
    static void YieldToHold();
    // 获取存在的协程数量
    static uint64_t TotalFiber();
    // 获取当前协程 id
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

// 当前线程正在执行的协程
static thread_local Fiber* t_fiber = nullptr;
// 当前线程的主协程
static thread_local Fiber::ptr t_master_fiber{};

// 协程栈大小配置项
static ConfigVar<uint64_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint64_t>("fiber.stack_size", 1024 * 1024);
} // namespace FiberInfo

} // namespace zjl

#endif
