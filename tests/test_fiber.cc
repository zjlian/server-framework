#include "src/fiber.h"
#include <iostream>
#include <memory>

int fib = 0;

void fiberFunc()
{
    std::cout << "调用 fiberFunc()" << std::endl;
    int a = 0;
    int b = 1;
    while (a < 20)
    {
        fib = a + b;
        a = b;
        b = fib;
        // 挂起当前协程
//        zjl::Fiber::GetThis()->swapOut();
        zjl::Fiber::YieldToReady();
    }
    std::cout << "fiberFunc() 结束" << std::endl;
}

int main(int, char**)
{
    zjl::Fiber::GetThis();
    auto fiber = std::make_shared<zjl::Fiber>(fiberFunc);
    std::cout << "换入协程，打印斐波那契数列" << std::endl;
    fiber->swapIn();
    while (fib < 100)
    {
        std::cout << fib << " ";
        fiber->swapIn();
    }
    std::cout << "完成" << std::endl;

    return 0;
}
