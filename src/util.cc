#include "util.h"
#include "fiber.h"
#include <execinfo.h>
#include <iostream>
#include <cxxabi.h>


namespace zjl
{

long GetThreadID()
{
    return ::syscall(SYS_gettid);
}

uint64_t GetFiberID()
{
    return Fiber::GetFiberID();
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
    for (int i = skip; string_list && i < call_stack_count; i++)
    {
        /**
         * 解码类型信息
         * 例如一个栈信息 ./test_exception(_Z2fni+0x62) [0x564e8a313317]
         * 函数签名在符号 "(" 后 "+" 前，
         * 调用 abi::__cxa_demangle 进行编码转换
        */
        std::stringstream ss;
        char* str = string_list[i];
        char* brackets_pos = nullptr;
        char* plus_pos = nullptr;
        // 找到左括号的位置
        for (brackets_pos = str; *brackets_pos != '(' && *brackets_pos; brackets_pos++)
        { /* do nothing */
        }
        assert(*brackets_pos == '(');
        // 先把到左括号的字符串塞进字符串流里
        *brackets_pos = '\0';
        ss << string_list[i] << '(';
        *brackets_pos = '(';
        // 找到加号的位置
        for (plus_pos = brackets_pos; *plus_pos != '+' && *plus_pos; plus_pos++)
        { /* do nothing */
        }
        // 解析类型信息
        char* type = nullptr;
        if (*brackets_pos + 1 != *plus_pos)
        {
            *plus_pos = '\0';
            int status = 0;
            type = abi::__cxa_demangle(brackets_pos + 1, nullptr, nullptr, &status);
            assert(status == 0 || status == -2);
            // 当 status == -2 时，意思是字符串解析错误，直接将原字符串塞进流里
            ss << (status == 0 ? type : brackets_pos + 1);
            *plus_pos = '+';
        }
        // 把剩下的也塞进去
        ss << plus_pos;
        out.push_back(ss.str());
        free(type);
    }
    // backtrace_symbols() 返回 malloc 分配的内存指针，需要 free
    free(string_list);
    free(void_ptr_list);
}
std::string BacktraceToString(int size, int skip)
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
