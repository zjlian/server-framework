#include "util.h"
#include <execinfo.h>
#include <iostream>
#include <sstream>

namespace zjl
{

int GetThreadID()
{
    return syscall(SYS_gettid);
}

int GetFiberID()
{
    return 0;
}

void Backtrace(std::vector<std::string>& out, int size, int skip)
{
    void** void_ptr_list = (void**)malloc(sizeof(void*) * size);
    int call_stack_count = ::backtrace(void_ptr_list, size);
    char** string_list = ::backtrace_symbols(void_ptr_list, call_stack_count);
    if (string_list == NULL)
    {
        std::cerr << "Backtrace() exception, 调用栈获取失败" << std::endl;
    }
    for (int i = skip; i < call_stack_count; i++)
    {
        out.push_back(string_list[i]);
    }
    // backtrace_symbols() 返回 malloc 分配的内存指针，需要 free
    free(string_list);
    free(void_ptr_list);
}
std::string BacktraceToSring(int size, int skip)
{
    std::vector<std::string> call_stack;
    Backtrace(call_stack, size, skip);
    std::stringstream ss;
    for (const auto& item : call_stack)
    {
        ss << item << std::endl;
    }
    return ss.str();
}

} // namespace zjl
