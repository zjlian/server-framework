#ifndef SERVER_FRAMEWORK_UTIL_H
#define SERVER_FRAMEWORK_UTIL_H

#include "noncopyable.h"
#include "singleton.h"
#include <cinttypes>
#include <memory>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace zjl
{

// 获取linux下线程的唯一id
int GetThreadID();
// 获取协程id
int GetFiberID();

} // namespace zjl
#endif
