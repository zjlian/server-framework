#include "src/fiber.h"
#include <iostream>
#include <memory>

int fib = 0;

void fiberFunc()
{
    int a = 0;
    int b = 1;
    while (a < 9999999)
    {
        fib = a + b;
        a = b;
        b = fib;
        // 挂起当前协程
        zjl::Fiber::GetThis()->swapOut();
    }
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
    return 0;
}
