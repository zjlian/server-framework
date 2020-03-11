#ifndef SERVER_FRAMEWORK_SCHEDULER_H
#define SERVER_FRAMEWORK_SCHEDULER_H

class uptr;
#include "fiber.h"
#include "thread.h"
#include <list>
#include <memory>
#include <utility>
#include <vector>

namespace zjl
{

/**
 * @brief 协程调度器
 * */
class Scheduler
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

        Fiber::uptr fiber;
        TaskFunc callback;
        long thread_id; // 任务要绑定执行线程的 id

        Task()
            : thread_id(-1) {}

        Task(Fiber::uptr f, long tid)
            : fiber(std::move(f)), thread_id(tid) {}

        Task(const TaskFunc& cb, long tid)
            : callback(cb), thread_id(tid) {}

        Task(TaskFunc&& cb, long tid)
            : callback(std::move(cb)), thread_id(tid) {}

        void reset()
        {
            fiber = nullptr;
            callback = nullptr;
            thread_id = -1;
        }
    };


public: // 内部类型、静态方法
    using ptr = std::shared_ptr<Scheduler>;
    using uptr = std::unique_ptr<Scheduler>;

    // 获取当前线程的调度器
    static Scheduler* GetThis();
    // 获取当前线程的 master fiber
    static Fiber* GetMasterFiber();

public: // 实例方法
    /**
     * @brief 构造函数
     * @param thread_size 线程池线程数量
     * @param use_caller 是否将 Scheduler 实例所在的线程作为任务执行环境
     * @param name 调度器名称
     * */
    Scheduler(size_t thread_size, bool use_caller = true, std::string name = "");
    virtual ~Scheduler();

    void start();
    void stop();

    /**
     * @brief 添加任务 thread-safe
     * @param Executable 模板类型必须是 std::unique_ptr<zjl::Fiber> 或者 std::function
     * @param exec Executable 的实例
     * @param thread_id 任务要绑定执行线程的 id
     * */
    template <typename Executable>
    void schedule(Executable&& exec, long thread_id = -1)
    {
        bool need_tickle = false;
        {
            ScopedLock lock(&m_mutex);
            // std::forward
            need_tickle = scheduleNonBlock(
                std::forward<Executable>(exec), thread_id);
        }
        if (need_tickle)
            tickle();
    }

protected:
    virtual void tickle() {};

private:
    /**
     * @brief 添加任务 non-thread-safe
     * @param Executable 模板类型必须是 std::unique_ptr<zjl::Fiber> 或者 std::function
     * @param exec Executable 的实例
     * @param thread_id 任务要绑定执行线程的 id
     * @return 是否是第一个新任务
     * */
    template <typename Executable>
    bool scheduleNonBlock(Executable&& exec, long thread_id = -1)
    {
        bool need_tickle = m_task_list.empty();
        // std::forward
        auto task = std::make_unique<Task>(
            std::forward<Executable>(exec), thread_id);
        // 创建的任务实例存在有效的 zjl::Fiber 或 std::function
        if (task->fiber || task->callback)
        {
            m_task_list.push_back(std::move(task));
        }
        return need_tickle;
    }

private:
    mutable Mutex m_mutex;
    const std::string m_name;
    // 线程集合
    std::vector<Thread::ptr> m_thread_list;
    // 任务集合
    std::list<Task::uptr> m_task_list;
};
} // namespace zjl

#endif //SERVER_FRAMEWORK_SCHEDULER_H
