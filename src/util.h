#ifndef SERVER_FRAMEWORK_UTIL_H
#define SERVER_FRAMEWORK_UTIL_H

#include "noncopyable.h"
#include "singleton.h"
#include <cinttypes>
#include <memory>
#include <pthread.h>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace zjl
{

// 获取linux下线程的唯一id
int GetThreadID();

// 获取协程id
int GetFiberID();

/**
 * @brief 以 vector 的形式获取调用栈
 * @param out 获取的调用栈
 * @param size 获取调用栈的最大层数，默认值为 200
 * @param skip 省略最近 n 层调用栈，默认值为 1，忽略获取 Backtrace() 本身的调用栈
*/
void Backtrace(std::vector<std::string>& out, int size = 200, int skip = 1);

/**
 * @brief 获取调用栈字符串，内部调用 Backtrace()
 * @param size 获取调用栈的最大层数，默认值为 200
 * @param skip 省略最近 n 层调用栈，默认值为 2，忽略获取 BacktraceToSring() 和 Backtrace() 的调用栈
*/
std::string BacktraceToSring(int size = 200, int skip = 2);

} // namespace zjl
#endif
