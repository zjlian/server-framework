#include "scheduler.h"
#include "log.h"

namespace zjl
{

static Logger::ptr g_logger = GET_LOGGER("system");
// 当前线程的协程调度器
static thread_local Scheduler* t_scheduler = nullptr;
// 协程调度器的调度工作协程
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler* Scheduler::GetThis()
{
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber()
{
    return t_scheduler_fiber;
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
        t_scheduler_fiber = m_root_fiber.get();
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
    LOG_DEBUG(g_logger, "调用 Scheduler::~Scheduler()");
    assert(m_auto_stop);
    if (GetThis() == this)
    {
        t_scheduler = nullptr;
    }
}

void Scheduler::start()
{
    LOG_DEBUG(g_logger, "调用 Scheduler::start()");
    { // !!! 作用域锁
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
    // m_root_fiber 存在就将它换入
    if (m_root_fiber)
    {
        m_root_fiber->swapIn();
    }
}

void Scheduler::stop()
{
    LOG_DEBUG(g_logger, "调用 Scheduler::stop()");
    m_auto_stop = true;
    // 实例化调度器时的参数 use_caller 为 true, 并且指定线程数量为 1 时
    // 说明只有当前一条主线程在执行，简单等待执行结束即可
    if (m_root_fiber &&
        m_thread_count == 0 &&
        (m_root_fiber->finish() || m_root_fiber->getState() == Fiber::INIT))
    {
        m_stopping = true;
        if (onStop())
            return;
    }
    //    bool exit_on_this_fiber = false;
    //    assert(m_root_thread_id == -1 && GetThis() != this);
    //    assert(m_root_thread_id != -1 && GetThis() == this);
    m_stopping = true;
    for (size_t i = 0; i < m_thread_count; i++)
    {
        tickle();
    }
    if (m_root_fiber)
    {
        tickle();
    }
    { // join 所有子线程
        for (auto& t : m_thread_list)
        {
            t->join();
        }
        m_thread_list.clear();
    }
    if (onStop())
    {
        return;
    }
}

bool Scheduler::isStop()
{
    // 调用过 Scheduler::stop()，并且任务列表没有新任务，也没有正在执行的协程，说明调度器已经彻底停止
    return m_auto_stop && m_task_list.empty() && m_active_thread_count == 0;
}

void Scheduler::tickle()
{
    //    LOG_DEBUG(g_logger, "调用 Scheduler::tickle()");
}

void Scheduler::run()
{
    LOG_DEBUG(g_logger, "调用 Scheduler::run()");
    t_scheduler = this;
    // 判断执行 run() 函数的线程，是否是线程池中的线程
    if (GetThreadID() != m_root_thread_id)
    { // 当前线程不存在 master fiber, 创建一个
        t_scheduler_fiber = Fiber::GetThis().get();
    }
    // 线程空闲时执行的协程
    auto idle_fiber = std::make_shared<Fiber>(
        std::bind(&Scheduler::onIdle, this));
    // 开始调度
    Task task;
    while (true)
    {
        task.reset();
        bool tickle_me = false;
        // 查找等待调度的 task
        { // !!! 作用域锁
            ScopedLock lock(&m_mutex);
            const auto& iter = m_task_list.begin();
            while (iter != m_task_list.end())
            {
                // 任务指定了要在那条线程执行，但当前线程不是指定线程，
                // 通知其他线程处理
                if ((*iter)->thread_id != -1 && (*iter)->thread_id != GetThreadID())
                {
                    tickle_me = true;
                    continue;
                }
                assert((*iter)->fiber || (*iter)->callback);
                // 任务是 fiber，但是是正在执行的，不进行处理
                if ((*iter)->fiber && (*iter)->fiber->getState() == Fiber::EXEC)
                {
                    continue;
                }
                // 找到可以执行的任务，拷贝一份
                task = **iter;
                ++m_active_thread_count;
                // 从任务列表里移除该任务
                m_task_list.erase(iter);
                break;
            }
        }
        if (tickle_me)
        {
            tickle();
        }
        if (task.callback)
        { // 如果是 callback 任务，为其创建 fiber
            task.fiber = std::make_shared<Fiber>(std::move(task.callback));
            task.callback = nullptr;
        }
        if (task.fiber && !task.fiber->finish())
        { // 是 fiber 任务
            if (GetThreadID() == m_root_thread_id)
            {
                // m_root_thread_id 等于当前线程 id，说明构造调度器时 use_caller 为 true
                // 使用 m_root_fiber 作为 master fiber
                task.fiber->swapIn(m_root_fiber);
            }
            else
            {
                task.fiber->swapIn();
            }
            --m_active_thread_count;
            // 协程换出后，继续将其添加到任务队列
            Fiber::State fiber_status = task.fiber->getState();
            if (fiber_status == Fiber::READY)
            {
                schedule(std::move(task.fiber), task.thread_id, true);
            }
            else if (fiber_status == Fiber::HOLD)
            {
                schedule(std::move(task.fiber));
            }
            task.reset();
        }
        else
        { // 任务队列空了，执行 idle_fiber
            if (idle_fiber->finish())
            {
                break;
            }
            ++m_idle_thread_count;
            if (GetThreadID() == m_root_thread_id)
            {
                // m_root_thread_id 等于当前线程 id，说明构造调度器时 use_caller 为 true
                // 使用 m_root_fiber 作为 master fiber
                idle_fiber->swapIn(m_root_fiber);
            }
            else if (m_root_thread_id == -1)
            {
                idle_fiber->swapIn();
            }
            --m_idle_thread_count;
        }
    }
    LOG_DEBUG(g_logger, "Scheduler::run() 结束");
}

} // namespace zjl
