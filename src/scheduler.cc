#include "scheduler.h"
#include "log.h"

namespace zjl
{

static Logger::ptr g_logger = GET_LOGGER("system");
// 当前线程的协程调度器
static thread_local Scheduler* t_scheduler = nullptr;

static thread_local Fiber* t_fiber = nullptr;

Scheduler* Scheduler::GetThis()
{
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber()
{
    return t_fiber;
}

Scheduler::Scheduler(size_t thread_size, bool use_caller, std::string name)
    : m_name(std::move(name))
{
    assert(thread_size > 0);
    if (use_caller)
    {
        // 实例化此类的线程作为 master fiber
        Fiber::GetThis();
        // 线程池需要的线程数减一
        --thread_size;
        // 确保该线程下只有一个调度器
        assert(GetThis() == nullptr);
        t_scheduler = this;
        // 因为 Scheduler::run 是实例方法，需要用 std::bind 绑定调用者
        m_root_fiber = std::make_shared<Fiber>(
            std::bind(&Scheduler::run, this));
        Thread::SetThisThreadName(m_name);
        t_fiber = m_root_fiber.get();
        m_root_thread_id = GetThreadID();
        m_thread_id_list.push_back(m_root_thread_id);
    }
    else
    {
        m_root_thread_id = -1;
    }
    m_thread_count = thread_size;
}

Scheduler::~Scheduler()
{
    assert(m_stopping);
    if (GetThis() == this)
    {
        t_scheduler = nullptr;
    }
}

void Scheduler::start()
{
    ScopedLock lock(&m_mutex);
    if (!m_stopping)
    { // 调度器已经开始工作
        return;
    }
    m_stopping = false;
    assert(m_thread_list.empty());
    m_thread_list.resize(m_thread_count);
    for (size_t i = 0; i < m_thread_count; i++)
    {
        m_thread_list[i] = std::make_shared<Thread>(
            std::bind(&Scheduler::run, this),
            m_name + "_" + std::to_string(i));
        m_thread_id_list.push_back(m_thread_list[i]->getId());
    }
}

void Scheduler::stop()
{
    m_auto_stop = true;
    // 实例化调度器时 use_caller 为 true, 线程数量为 1
    // 简单等待执行结束即可
    bool root_fiber_not_work =
        m_root_fiber->finish() || m_root_fiber->getState() == Fiber::INIT;
    if (m_root_fiber &&
        m_thread_count == 0 &&
        root_fiber_not_work)
    {
        m_stopping = true;
        if (onStop())
            return;
    }
//    bool exit_on_this_fiber = false;
    assert(m_root_thread_id == -1 && GetThis() == nullptr);
    assert(m_root_thread_id != -1 && GetThis() == this);
    m_stopping = true;
    for (size_t i = 0; i < m_thread_count; i++)
    {
        tickle();
    }
    if (m_root_fiber)
    {
        tickle();
    }
    if (onStop())
    {
        return;
    }
}

void Scheduler::tickle()
{
}

void Scheduler::run()
{
}

} // namespace zjl