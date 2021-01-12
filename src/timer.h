#ifndef SERVER_FRAMEWORK_TIMER_H
#define SERVER_FRAMEWORK_TIMER_H

#include <set>
#include <vector>
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

    /**
     * @brief 取消定时器
    */
    bool cancel();

    /**
     * @brief 重设定时间隔
     * @param ms 延迟
     * @param from_now 是否立即开始倒计时
    */
    bool reset(uint64_t ms, bool from_now);

    /**
     * @brief 重新计时
    */
    bool refresh();

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

    /**
     * @brief 用于创建只有时间信息的定时器，基本是用于查找超时的定时器，无其他作用
    */
    Timer(uint64_t next);

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

    /**
     * @brief 获取下一个定时器的等待时间
     * @return 返回结果分为三种：无定时器等待执行返回 ~0ull，存在超时未执行的定时器返回 0，存在等待执行的定时器返回剩余的等待时间
    */
    uint64_t getNextTimer();

    /**
     * @brief 获取所有等待超时的定时器的回调函数对象，并将定时器从队列中移除，这个函数会自动将周期调用的定时器存回队列
    */
    void listExpiredCallback(std::vector<std::function<void()>>& fns);

    /**
     * @brief 检查是否有等待执行的定时器
    */
    bool hasTimer();

protected:
    /**
     * @brief 当创建了延迟时间最短的定时任务时，会调用此函数
    */
    virtual void onTimerInsertedAtFirst() = 0;

    /**
     * @brief 添加已有的定时器对象，该函数只是为了代码复用
    */
    void addTimer(Timer::ptr timer, WriteScopedLock& lock);

private:
    /**
     * @brief 检查系统时间是否被修改成更早的时间
    */
    bool detectClockRollover(uint64_t now_ms);

private:
    RWLockType m_lock;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    uint64_t m_previous_time = 0;
};

} // end namespace zjl



#endif
