#include "src/log.h"
#include "src/thread.h"
#include <memory>
#include <stdint.h>
#include <unistd.h>
#include <vector>
auto g_logger = GET_ROOT_LOGGER();

static uint64_t count = 0;
zjl::RWLock s_rwlock;
zjl::Mutex s_mutex;

void fn_1()
{
    LOG_FMT_DEBUG(
        g_logger,
        "当前线程 id = %ld/%d, 当前线程名 = %s",
        zjl::GetThreadID(),
        zjl::Thread::GetThisId(),
        zjl::Thread::GetThisThreadName().c_str());
}

void fn_2()
{
    zjl::WriteScopedLock rsl(&s_rwlock);
    for (int i = 0; i < 100000000; i++)
    {
        count++;
    }
}

// 测试线程创建
void TEST_createThread()
{
    LOG_DEBUG(g_logger, "Call TEST_createThread() 测试线程创建");
    std::vector<zjl::Thread::ptr> thread_list;
    for (size_t i = 0; i < 5; ++i)
    {
        thread_list.push_back(
            std::make_shared<zjl::Thread>(&fn_1, "thread_" + std::to_string(i)));
    }
    LOG_DEBUG(g_logger, "调用 join() 启动子线程，将子线程并入主线程");
    for (auto thread : thread_list)
    {
        thread->join();
    }
    LOG_DEBUG(g_logger, "创建子线程，使用析构函数调用 detach() 分离子线程");
    for (size_t i = 0; i < 5; ++i)
    {
        std::make_unique<zjl::Thread>(&fn_1, "detach_thread_" + std::to_string(i));
    }
}

void TEST_readWriteLock()
{
    sleep(0);
    LOG_DEBUG(g_logger, "Call TEST_readWriteLock() 测试线程读写锁");
    std::vector<zjl::Thread::uptr> thread_list;
    for (int i = 0; i < 10; i++)
    {
        thread_list.push_back(
            std::make_unique<zjl::Thread>(&fn_2, "temp_thread" + std::to_string(i)));
    }

    for (auto& thread : thread_list)
    {
        thread->join();
    }

    LOG_FMT_DEBUG(g_logger, "count = %ld", count);
}

int main()
{
    TEST_createThread();
    TEST_readWriteLock();

    // sleep(3);
    return 0;
}
