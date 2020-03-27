#include "src/scheduler.h"
#include <iostream>

void fn (int count)
{
    for (int i = 0; i < count; i++)
        std::cout << "fiber test" <<  std::endl;
}

int main(int, char**)
{
    zjl::Scheduler sc(1);
    sc.schedule(std::bind(fn, 6));
    sc.start();
    sc.stop();
    return 0;
}