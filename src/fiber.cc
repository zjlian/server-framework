#include "fiber.h"
#include "exception.h"
#include "log.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <utility>

namespace zjl
{

/**
 * @brief 对 malloc/free 简单封装的内存分配器
*/
class MallocStackAllocator
{
public:
    static void* Alloc(uint64_t size)
    {
        return ::malloc(size);
    }

    static void Dealloc(void* ptr, uint64_t size)
    {
        free(ptr);
    }
};

/**
 * @brief 协程栈空间分配器
*/
using StackAllocator = MallocStackAllocator;

/**
 * ===============================
 * Fiber 的实现
 * ===============================
*/

Fiber::Fiber()
    : m_id(0),
      m_stack_size(0),
      m_state(EXEC),
      m_ctx(),
      m_stack(nullptr),
      m_callback()
{
    SetThis(this);
    // 获取上下文对象的副本
    if (getcontext(&m_ctx))
    {
        throw Exception(std::string(::strerror(errno)));
    }
    // 存在协程数量增加
    ++FiberInfo::s_fiber_count;
}

Fiber::Fiber(FiberFunc callback, size_t stack_size)
    : m_id(++FiberInfo::s_fiber_id),
      m_stack_size(stack_size),
      m_state(INIT),
      m_ctx(),
      m_stack(nullptr),
      m_callback(std::move(callback))
{
    // 如果传入的 stack_size 为 0，使用配置项 "fiber.stack_size" 设置的值
    if (m_stack_size == 0)
    {
        m_stack_size = FiberInfo::g_fiber_stack_size->getValue();
    }
    // 获取上下文对象的副本
    if (getcontext(&m_ctx))
    {
        throw Exception(std::string(::strerror(errno)));
    }
    // 给上下文对象分配分配新的栈空间内存
    m_stack = StackAllocator::Alloc(m_stack_size);
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stack_size;
    // 给新的上下文绑定入口函数
    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    ++FiberInfo::s_fiber_count;
}

Fiber::~Fiber()
{
    if (m_stack) // 存在栈，说明是子协程，释放申请的协程栈空间
    {
        // 只有子协程未被启动或者执行结束，才能被析构，否则属于程序错误
        assert(m_state == INIT || m_state == TERM || m_state == EXCEPTION);
        StackAllocator::Dealloc(m_stack, m_stack_size);
    }
    else // 否则是 master fiber
    {
        // master fiber 不存在执行函数
        assert(!m_callback);
        assert(m_state == EXEC);
        if (FiberInfo::t_fiber == this)
        {
            SetThis(nullptr);
        }
    }
}

void Fiber::reset(FiberFunc callback)
{
    assert(m_stack);
    assert(m_state == INIT || m_state == TERM || m_state == EXCEPTION);
    m_callback = std::move(callback);
    if (getcontext(&m_ctx))
    {
        throw Exception(std::string(::strerror(errno)));
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stack_size;
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}

void Fiber::swapIn()
{
    // 只有线程是等待执行的状态才能被换入
    assert(m_state == INIT || m_state == READY || m_state == HOLD);
    SetThis(this);
    m_state = EXEC;
    // 挂起 master fiber，切换到当前 fiber
    if (swapcontext(&(FiberInfo::t_master_fiber->m_ctx), &m_ctx))
    {
        throw Exception(std::string(::strerror(errno)));
    }
}

void Fiber::swapOut()
{
    assert(m_stack);
    SetThis(FiberInfo::t_master_fiber.get());
    // 挂起当前 fiber，切换到 master fiber
    if (swapcontext(&m_ctx, &(FiberInfo::t_master_fiber->m_ctx)))
    {
        throw Exception(std::string(::strerror(errno)));
    }
}

Fiber::ptr Fiber::GetThis()
{
    if (FiberInfo::t_fiber)
    {
        // 调用 std::enable_shared_from_this::shared_from_this() 获取对象 this 的智能指针
        return FiberInfo::t_fiber->shared_from_this();
    }
    // 当 FiberInfo::t_fiber 是 nullptr 时，说明该线程不存在 master fiber
    // 初始化 master_fiber
    Fiber::ptr first_fiber(new Fiber());
    FiberInfo::t_master_fiber.swap(first_fiber);
    return FiberInfo::t_master_fiber->shared_from_this();
}

void Fiber::SetThis(Fiber* fiber)
{
    FiberInfo::t_fiber = fiber;
}

void Fiber::YieldToReady()
{
    auto current_fiber = GetThis();
    current_fiber->m_state = READY;
    current_fiber->swapOut();
}

void Fiber::YieldToHold()
{
    auto current_fiber = GetThis();
    current_fiber->m_state = HOLD;
    current_fiber->swapOut();
}

uint64_t Fiber::TotalFiber()
{
    return FiberInfo::s_fiber_count;
}

uint64_t Fiber::GetFiberID()
{
    if (FiberInfo::t_fiber)
    {
        return FiberInfo::t_fiber->getID();
    }
    return 0;
}

void Fiber::MainFunc()
{
    auto current_fiber = GetThis();
    auto logger = GET_LOGGER("system");
    try
    {
        current_fiber->m_callback();
        current_fiber->m_callback = nullptr;
        current_fiber->m_state = TERM;
    }
    catch (zjl::Exception& e)
    {
        LOG_FMT_ERROR(
            logger,
            "Fiber exception: %s, call stack:\n%s",
            e.what(),
            e.stackTrace());
    }
    catch (std::exception& e)
    {
        LOG_FMT_ERROR(logger, "Fiber exception: %s", e.what());
    }
    catch (...)
    {
        LOG_ERROR(logger, "Fiber exception");
    }
    // 执行结束后，切回主协程
    Fiber* current_fiber_ptr = current_fiber.get();
    // 释放所有权
    current_fiber.reset();
    current_fiber_ptr->swapOut();
}

} // namespace zjl