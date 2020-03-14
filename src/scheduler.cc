#include "scheduler.h"
#include "log.h"

namespace zjl
{

static Logger::ptr g_logger = GET_LOGGER("system");
// 当前线程的协程调度器
static thread_local Scheduler* t_scheduler = nullptr;
// 当前现在上正在跑的协程
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
    t_scheduler = this;
    if (GetThreadID() != m_root_thread_id)
    { // 当前线程不存在 master fiber, 创建一个
        t_fiber = Fiber::GetThis().get();
    }
    // 线程空闲时执行的协程
    auto idle_fiber = std::make_shared<Fiber>(
        std::bind(&Scheduler::onIdle, this));
    // 执行 std::function 的协程的智能指针，延迟创建实例
    Fiber::uptr callback_fiber;
    /**
     * 调度逻辑
     * */
    Task task;
    while (true)
    {
        task.reset();
        bool tickle_me = false;
        {   // !!! 作用域锁
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
                // 从任务列表里移除该任务
                m_task_list.erase(iter);
                break;
            }
        }
        if (tickle_me)
        {
            tickle();
        }
        assert(task.fiber || task.callback);
        if (task.fiber && !task.fiber->finish()) // 是 fiber 任务
        {
            ++m_active_thread_count;
            task.fiber->swapIn();
            --m_active_thread_count;
            // 协程换出后，继续将其添加到任务队列
            auto fiber_status = task.fiber->getState();
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
        else if (task.callback) // 是 std::function 任务，让 callback_fiber 来执行
        {
            if (callback_fiber) // callback_fiber 存在，设置执行函数
            {
                callback_fiber->reset(std::move(task.callback));
            }
            else // 不存在就实例化一个
            {
                callback_fiber = std::make_unique<Fiber>(task.callback);
            }
            task.reset();
            ++m_active_thread_count;
            callback_fiber->swapIn();
            --m_active_thread_count;
            // callback_fiber 换出后
            if (!callback_fiber->finish()) // callback_fiber 未完成，将其加入任务队列
            {
                schedule(std::move(callback_fiber));
            }
            callback_fiber.reset();
        }
        else // 任务队列空了，执行 idle_fiber
        {
            if (idle_fiber->finish())
            {
                break;
            }
            ++m_idle_thread_count;
            idle_fiber->swapIn();
            --m_idle_thread_count;
        }
    }
}

} // namespace zjl