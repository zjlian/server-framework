#include "src/fiber.h"
#include <iostream>
#include <memory>
#include <cstdio>
#include <array>

int fib = 0;

class Fvck
{
public:
    Fvck()
    {
        std::cout << "构造对象 Fvck" << std::endl;
    }

    ~Fvck()
    {
        std::cout << "析构对象 Fvck" << std::endl;
    }
};

void fiberFunc()
{
    std::array<Fvck, 3> list;
    std::cout << "调用 fiberFunc()" << std::endl;
    int a = 0;
    int b = 1;
    while (a < 20)
    {
        fib = a + b;
        a = b;
        b = fib;
        // 挂起当前协程
        zjl::Fiber::YieldToReady();
    }
    std::cout << "fiberFunc() 结束" << std::endl;
}

void test(char c)
{
    for (int i = 0; i < 10; i++)
    {
        std::cout << c << std::endl;
        zjl::Fiber::YieldToReady();
    }
}

int main(int, char**)
{
   zjl::Fiber::GetThis();
   {
       auto fiber = std::make_shared<zjl::Fiber>(fiberFunc);
       std::cout << "换入协程，打印斐波那契数列" << std::endl;
       fiber->swapIn();
       while (fib < 100 && !fiber->finish())
       {
           std::cout << fib << " ";
           fiber->swapIn();
       }
   }
//    std::cout << "完成" << std::endl;
    return 0;
}
