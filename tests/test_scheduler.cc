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

int main(int, char**)
{
    zjl::Scheduler sc(3, false);
    sc.start();
    sc.schedule(std::bind(fn, "协程 0"));
    sc.schedule(std::bind(fn, "协程 1"));
    sc.schedule(std::bind(fn, "协程 2"));
    sc.schedule(std::bind(fn, "协程 3"));
    sleep(1);
    sc.stop();
    return 0;
}