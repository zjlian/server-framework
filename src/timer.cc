#include "timer.h"

namespace zjl 
{

bool Timer::Comparator::operator()(
    const Timer::ptr& lhs, const Timer::ptr& rhs) const
{
    // 判断指针的有效性
    if (!lhs && !rhs)
        return false;
    if (!lhs) 
        return true;
    if (!rhs)
        return false;
    // 按绝对时间戳排序
    if (lhs->m_next < rhs->m_next)
    {
        return true;
    }
    else if (rhs->m_next < lhs->m_next)
    {
        return false;
    }
    // 时间戳相同就按指针排序
    return lhs.get() < rhs.get();
}

Timer::Timer(
    uint64_t ms, std::function<void()> fn, bool cyclic, TimerManager* manager)
    : m_cyclic(cyclic), 
      m_ms(ms), 
      m_fn(fn),
      m_manager(manager)
{
    m_next = GetCurrentMS() + m_ms;
}

Timer::Timer(uint64_t next) : m_next(next)
{}

bool Timer::cancel()
{
    WriteScopedLock lock(&m_manager->m_lock);
    if (m_fn)
    {
        m_fn = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::reset(uint64_t ms, bool from_now)
{
    if (ms == m_ms && !from_now)
    {
        return true;
    }
    if (!m_fn)
    {
        return false;
    }
    WriteScopedLock lock(&m_manager->m_lock);
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end())
    {
        return false;
    }
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    // 重新计时
    if (from_now)
    {
        start = GetCurrentMS();
    }
    else 
    {
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    // it = m_manager->m_timers.insert(shared_from_this()).first;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}
 
bool Timer::refresh()
{
    WriteScopedLock lock(&m_manager->m_lock);
    if (!m_fn)
    {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end())
    {
        return false;
    }
    m_manager->m_timers.erase(it);
    m_next = GetCurrentMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

TimerManager::TimerManager()
{
    m_previous_time = GetCurrentMS();
}

TimerManager::~TimerManager()
{ 
}

Timer::ptr TimerManager::addTimer(
    uint64_t ms, std::function<void()> fn, bool cyclic)
{
    Timer::ptr timer(new Timer(ms, fn, cyclic, this)); 
    WriteScopedLock lock(&m_lock);
    addTimer(timer, lock);
    return timer;
}

void TimerManager::addTimer(Timer::ptr timer, WriteScopedLock& lock)
{
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin());
    lock.unlock();
    if (at_front)
    {
        onTimerInsertedAtFirst();
    }
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> fn)
{
    auto tmp = weak_cond.lock();
    if (tmp)
    {
        fn();
    }
}

Timer::ptr TimerManager::addConditionTimer(
    uint64_t ms, std::function<void()> fn, 
    std::weak_ptr<void> weak_cond, bool cyclic)
{
    return addTimer(ms, std::bind(&OnTimer, weak_cond, fn), cyclic);
}

uint64_t TimerManager::getNextTimer()
{
    ReadScopedLock lock(&m_lock);
    if (m_timers.empty())
    {
        // 没有定时器
        return ~0ull;
    }
    const Timer::ptr& next = *m_timers.begin();
    uint64_t now_ms = GetCurrentMS();
    if (now_ms >= next->m_next)
    {
        // 等待超时
        return 0;
    }
    else 
    {
        // 返回剩余的等待时间
        return next->m_next - now_ms;
    }
}

void TimerManager::listExpiredCallback(std::vector<std::function<void()>>& fns)
{
    uint64_t now_ms = GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        ReadScopedLock lock(&m_lock);
        if (m_timers.empty())
        {
            return;
        }
    }
    WriteScopedLock lock(&m_lock);
    // 检查系统时间是否被修改
    bool rollover = detectClockRollover(now_ms);
    // 系统时间未被回拨，并且无定时器等待超时
    if (!rollover && (*m_timers.begin())->m_next > now_ms)
    {
        return;
    }
    Timer::ptr now_timer(new Timer(now_ms));
    // 获取第一个 m_next 大于或等于 now_timer->m_next 的定时器的迭代器
    // 就是已经等待到达或超时的定时器。
    // ** 如果系统时间被修改过，直接认定所有定时器均超时 **
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    // 包括上到达指定时间的定时器
    while (it != m_timers.end() && (*it)->m_next == now_timer->m_next)
    {
        ++it;
    }
    // 取出超时的定时器
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    fns.reserve(expired.size());
    for (auto& timer : expired)
    {
        fns.push_back(timer->m_fn);
        // 处理周期定时器
        if (timer->m_cyclic)
        {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }
        else
        {
            timer->m_fn = nullptr;
        }
    }    
}

bool TimerManager::hasTimer() 
{
    ReadScopedLock lock(&m_lock);
    return !m_timers.empty();
}

bool TimerManager::detectClockRollover(uint64_t now_ms)
{
    bool rollover = false;
    // 系统时间被回拨超过一个小时
    if (now_ms < m_previous_time && 
        now_ms < (m_previous_time - 60 * 60 * 1000))
    {
        rollover = true;
    }
    m_previous_time = now_ms;
    return rollover;
}

}
