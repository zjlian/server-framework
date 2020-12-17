#ifndef SERVER_FRAMEWORK_HOOK_H
#define SERVER_FRAMEWORK_HOOK_H

#include <unistd.h>

namespace zjl
{

/**
 * @brief 检查当前线程是否开启系统函数 hook 到协程的实现
*/
bool isHookEnabled();

/**
 * @brief 开启系统函数 hook 到协程的实现
*/
void setHookEnable(bool flag);

} // namespace zjl

extern "C" 
{
typedef unsigned int (*sleep_func)(unsigned int seconds);
extern sleep_func sleep_f;

typedef int (*usleep_func)(useconds_t usec);
extern usleep_func usleep_f;
}

#endif
