#include "src/exception.h"
#include <iostream>

void fn(int count)
{
    if (count <= 0)
    {
        throw zjl::Exception("fn 递归结束");
    }
    fn(count - 1);
}

int main()
{
    try
    {
        fn(10);
    }
    catch (const zjl::Exception& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << e.stackTrace() << std::endl;
    }

    return 0;
}
