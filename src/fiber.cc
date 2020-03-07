#include "fiber.h"
#include "config.h"
#include <atomic>

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
 * ===============================
 * Fiber 的实现
 * ===============================
*/

Fiber::Fiber(FiberFunc callback, size_t stack_size)
{
}

Fiber::~Fiber()
{
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

void Fiber::YieldToReady()
{
}

void Fiber::YieldToHold()
{
}

uint64_t Fiber::TotalFiber()
{
}

} // namespace zjl
