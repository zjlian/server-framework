#include "hook.h"
#include "io_manager.h"
#include "log.h"
#include <dlfcn.h>

namespace zjl
{

static thread_local bool t_hook_enabled = false;

#define DEAL_FUNC(DO) \
    DO(sleep) \
    DO(usleep) \
    DO(nanosleep)

void hook_init()
{
    static bool is_inited = false;
    if (is_inited)
        return;
    
#define TRY_LOAD_HOOK_FUNC(name) name##_f = (name##_func)dlsym(RTLD_NEXT, #name);
    DEAL_FUNC(TRY_LOAD_HOOK_FUNC)
#undef TRY_LOAD_HOOK_FUNC
}

struct _HookIniter
{
    _HookIniter()
    { hook_init(); }
};
static _HookIniter s_hook_initer;

bool isHookEnabled()
{
    return t_hook_enabled;
}

void setHookEnable(bool flag)
{
    t_hook_enabled = flag; 
}

} // namespace zjl

extern "C" 
{
#define DEF_FUNC_NAME(name) name##_func name##_f = nullptr;
    DEAL_FUNC(DEF_FUNC_NAME)
#undef DEF_FUNC_NAME

/**
 * @brief hook 处理后的 sleep
*/
unsigned int sleep(unsigned int seconds)
{
    if (!zjl::t_hook_enabled)
    {
        return sleep_f(seconds);
    }
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(seconds * 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

/**
 * @brief hook 处理后的 usleep
*/
int usleep(useconds_t usec)
{
    if (!zjl::t_hook_enabled)
    {
        return usleep_f(usec);
    }
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(usec / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!zjl::t_hook_enabled)
    {
        return nanosleep_f(req, rem);
    }
    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    zjl::Fiber::ptr fiber = zjl::Fiber::GetThis();
    auto iom = zjl::IOManager::GetThis();
    assert(iom != nullptr && "这里的 IOManager 指针不可为空");
    iom->addTimer(timeout_ms, [iom, fiber](){
        iom->schedule(fiber);
    });
    zjl::Fiber::YieldToHold();
    return 0;
}

}

typedef unsigned int (*sleep_func)(unsigned int seconds);
extern sleep_func sleep_f;

typedef int (*usleep_func)(useconds_t usec);
extern usleep_func usleep_f;
