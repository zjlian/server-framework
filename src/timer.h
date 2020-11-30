#ifndef SERVER_FRAMEWORK_TIMER_H
#define SERVER_FRAMEWORK_TIMER_H

#include <set>
#include <memory>
#include "thread.h"

namespace zjl 
{

class TimerManager;

/**
 * @brief 定时器类
*/
class Timer : public std::enable_shared_from_this<Timer>
{
friend class TimerManager;

public:
    typedef std::shared_ptr<Timer> ptr;

private:
    /**
     * @brief Constructor
     * @param ms 延迟时间
     * @param fn 回调函数
     * @param cyclic 是否重复执行
     * @param manager 执行环境
    */
    Timer(uint64_t ms, std::function<void()> fn, 
        bool cyclic, TimerManager* manager);

private:
    bool m_cyclic = false;  // 是否重复
    uint64_t m_ms = 0;      // 执行周期
    uint64_t m_next = 0;    // 执行的绝对时间戳
    std::function<void()> m_fn;
    TimerManager* m_manager = nullptr;

private:
    struct Comparator {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

/**
 * @brief 定时器调度类
*/
class TimerManager  
{
friend class Timer;
public:
    using RWLockType = RWLock;

    TimerManager();
    virtual ~TimerManager();

    /**
     * @brief 新增一个普通定时器
     * @param ms 延迟毫秒数
     * @param fn 回调函数
     * @param weak_cond 执行条件
     * @param cyclic 是否重复执行
    */
    Timer::ptr addTimer(uint64_t ms, std::function<void()> fn, bool cyclic = false);

    /**
     * @brief 新增一个条件定时器。当到达执行时间时，提供的条件变量依旧有效，则执行，否则不执行
     * @param ms 延迟毫秒数
     * @param fn 回调函数
     * @param weak_cond 条件变量，利用智能指针是否有效作为判断条件
     * @param cyclic 是否重复执行
    */
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> fn,
        std::weak_ptr<void> weak_cond, bool cyclic = false);

protected:
    /**
     * @brief 当创建了延迟时间最短的定时任务时，会调用此函数
    */
    virtual void onTimerInsertedAtFirst() = 0;

private:
    RWLockType m_lock;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
};

} // end namespace zjl



#endif
