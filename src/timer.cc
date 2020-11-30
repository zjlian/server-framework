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

TimerManager::TimerManager()
{

}

TimerManager::~TimerManager()
{

}

Timer::ptr TimerManager::addTimer(
    uint64_t ms, std::function<void()> fn, bool cyclic = false)
{
    Timer::ptr timer = std::make_shared<Timer>(ms, fn, cyclic);
    WriteScopedLock lock(&m_lock);
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin());
    lock.unlock();
    if (at_front)
    {
        onTimerInsertedAtFirst();
    }
}

Timer::ptr TimerManager::addConditionTimer(
    uint64_t ms, std::function<void()> fn, 
    std::weak_ptr<void> weak_cond, bool cyclic = false)
{
    Timer::ptr timer = std::make_shared<Timer>(ms, fn, cyclic);
}

}
