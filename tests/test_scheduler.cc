#include "src/log.h"
#include "src/scheduler.h"
#include <iostream>

void fn(const char* msg)
{
    for (int i = 0; i < 4; i++)
    {
        LOG_FMT_INFO(GET_ROOT_LOGGER(), "%s : %d", msg, i);
        zjl::Fiber::YieldToHold();
    }
}

void fn2(uint32_t t, int32_t c)
{
    sleep(t);
    LOG_FMT_INFO(GET_ROOT_LOGGER(), "delayed: %d, count: %d", t, c);
    if (c > 0)
    {
        zjl::Scheduler::GetThis()->schedule(std::bind(fn2, t, c - 1));
    }
}

int main(int, char**)
{
    zjl::Scheduler sc(3, false);
    sc.start();
    sc.schedule(std::bind(fn2, 1, 4));
    sc.schedule(std::bind(fn2, 2, 4));
    sc.schedule(std::bind(fn2, 3, 4));
    sc.schedule(std::bind(fn, "协程 0"));
    sc.schedule(std::bind(fn, "协程 1"));
    sc.schedule(std::bind(fn, "协程 2"));
    sc.schedule(std::bind(fn, "协程 3"));
    sc.stop();
    return 0;
}