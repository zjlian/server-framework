#ifndef SERVER_FRAMEWORK_SCHEDULER_H
#define SERVER_FRAMEWORK_SCHEDULER_H

#include "fiber.h"
#include "thread.h"
#include <atomic>
#include <list>
#include <memory>
#include <unistd.h>
#include <utility>
#include <vector>

namespace zjl
{

/**
 * @brief 协程调度器
 * */
class Scheduler : public noncopyable
{
private: // 内部类
    /**
     * @brief 任务类
     * 等待分配线程执行的任务，可以是 zjl::Fiber 或 std::function
     * */
    struct Task
    {
        using ptr = std::shared_ptr<Task>;
        using uptr = std::unique_ptr<Task>;
        using TaskFunc = std::function<void()>;

        Fiber::ptr fiber;
        TaskFunc callback;
        long thread_id; // 任务要绑定执行线程的 id

        Task()
            : thread_id(-1) {}

        Task(const Task& rhs) = default;

        Task(Fiber::ptr f, long tid)
            : fiber(std::move(f)), thread_id(tid) {}

        Task(const TaskFunc& cb, long tid)
            : callback(cb), thread_id(tid) {}

        Task(TaskFunc&& cb, long tid)
            : callback(std::move(cb)), thread_id(tid) {}

        Task& operator=(const Task& rhs) = default;

        void reset()
        {
            fiber = nullptr;
            callback = nullptr;
            thread_id = -1;
        }
    };

public: // 内部类型、静态方法、友元声明
    friend class Fiber;
    using ptr = std::shared_ptr<Scheduler>;
    using uptr = std::unique_ptr<Scheduler>;

    // 获取当前协程的调度器
    static Scheduler* GetThis();
    // 获取调度器的调度工作协程
    static Fiber* GetMainFiber();

public: // 实例方法
    /**
     * @brief 构造函数
     * @param thread_size 线程池线程数量
     * @param use_caller 是否将 Scheduler 实例化所在的线程作为 master fiber
     * @param name 调度器名称
     * */
    explicit Scheduler(size_t thread_size, bool use_caller = true, std::string name = "");
    virtual ~Scheduler();

    void start();
    void stop();
    virtual bool isStop();
    bool hasIdleThread() const
    {
        return m_idle_thread_count > 0;
    }

    /**
     * @brief 添加任务 thread-safe
     * @param Executable 模板类型必须是 std::unique_ptr<zjl::Fiber> 或者 std::function
     * @param exec Executable 的实例
     * @param instant 是否优先调度
     * @param thread_id 任务要绑定执行线程的 id
     * */
    template <typename Executable>
    void schedule(Executable&& exec, long thread_id = -1, bool instant = false)
    {
        bool need_tickle = false;
        {
            ScopedLock lock(&m_mutex);
            // std::forward
            need_tickle = scheduleNonBlock(std::forward<Executable>(exec), thread_id);
        }
        // 该工作了
        if (need_tickle)
            tickle();
    }

protected:
    void run();
    virtual void tickle();
    // 调度器停止时的回调函数，返回调度器当前是否处于停止工作的状态
    virtual bool onStop() { return isStop(); }
    // 调度器空闲时的回调函数
    virtual void onIdle()
    {
        while (!isStop())
        {
            Fiber::YieldToHold();
        }
        return;
    }

private:
    /**
     * @brief 添加任务 non-thread-safe
     * @param Executable 模板类型必须是 std::unique_ptr<zjl::Fiber> 或者 std::function
     * @param exec Executable 的实例
     * @param thread_id 任务要绑定执行线程的 id
     * @param instant 是否优先调度
     * @return 是否是空闲状态下的第一个新任务
     * */
    template <typename Executable>
    bool scheduleNonBlock(Executable&& exec, long thread_id = -1, bool instant = false)
    {
        bool need_tickle = m_task_list.empty();
        // std::forward
        auto task = std::make_unique<Task>(std::forward<Executable>(exec), thread_id);
        // 创建的任务实例存在有效的 zjl::Fiber 或 std::function
        if (task->fiber || task->callback)
        {
            if (instant)
                m_task_list.push_front(std::move(task));
            else
                m_task_list.push_back(std::move(task));
        }
        return need_tickle;
    }

protected:
    const std::string m_name;
    // 主线程 id，仅在 use_caller 为 true 时会被设置有效线程 id
    long m_root_thread_id = 0;
    // 线程 id 列表
    std::vector<long> m_thread_id_list;
    // 有效线程数量
    size_t m_thread_count = 0;
    // 活跃线程数量
    std::atomic_uint64_t m_active_thread_count{};
    // 空闲线程数量
    std::atomic_uint64_t m_idle_thread_count{};
    // 执行停止状态
    bool m_stopping = true;
    // 是否自动停止
    bool m_auto_stop = false;

private:
    mutable Mutex m_mutex;
    // 负责调度的协程，仅在类实例化参数中 use_caller 为 true 时有效
    Fiber::ptr m_root_fiber;
    // 线程对象列表
    std::vector<Thread::ptr> m_thread_list;
    // 任务集合
    std::list<Task::ptr> m_task_list;
};
} // namespace zjl

#endif //SERVER_FRAMEWORK_SCHEDULER_H
