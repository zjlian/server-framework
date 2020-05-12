#include "src/log.h"
#include "src/scheduler.h"
#include <iostream>

void fn()
{
    for (int i = 0; i < 3; i++)
    {
        std::cout << "啊啊啊啊啊啊" << std::endl;
        zjl::Fiber::YieldToHold();
    }
}

void fn2()
{
    for (int i = 0; i < 3; i++)
    {
        std::cout << "哦哦哦哦哦哦" << std::endl;
        zjl::Fiber::YieldToHold();
    }
}

int main(int, char**)
{
    zjl::Scheduler sc(1, false);
    sc.start();

    int i = 0;
    for (i = 0; i < 3; i++)
    {
        sc.schedule([&i]() {
            std::cout << ">>>>>> " << i << std::endl;
        });
    }

    sc.stop();
    return 0;
}