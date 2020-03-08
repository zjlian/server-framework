#include "fiber.h"
#include "config.h"
#include "exception.h"
#include <assert.h>
#include <atomic>
#include <cstring>
#include <errno.h>

namespace zjl
{

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
    if (getcontext(&m_ctx) == -1)
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
      m_callback(callback)
{
    // 如果传入的 stack_size 为 0，使用配置项 "fiber.stack_size" 设置的值
    if (m_stack_size == 0)
    {
        m_stack_size = FiberInfo::g_fiber_stack_size->getValue();
    }
    // 获取上下文对象的副本
    if (getcontext(&m_ctx) == -1)
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
    if (m_stack) // 存在栈，说明是子协程
    {
        // 只有子协程未被启动或者执行结束，才能被析构，否则属于程序错误
        assert(m_state == INIT || m_state == TERM);
        StackAllocator::Dealloc(m_stack, m_stack_size);
    }
    else // 否则是主协程
    {
        assert(!m_callback);
        assert(m_state == EXEC);
        Fiber* current_fiber = FiberInfo::t_fiber;
        if (current_fiber == this)
        {
            SetThis(nullptr);
        }
    }
}

void Fiber::reset(FiberFunc callback)
{
}

void Fiber::swapIn()
{
}

void Fiber::swapOut()
{
}

Fiber::ptr Fiber::GetThis()
{
}

void SetThis(Fiber* fiber)
{
}

void Fiber::YieldToReady()
{
}

void Fiber::YieldToHold()
{
}

uint64_t Fiber::TotalFiber()
{
}

void Fiber::MainFunc()
{
}

} // namespace zjl
