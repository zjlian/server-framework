#include "util.h"

namespace zjl {

int GetThreadID() {
    return syscall(SYS_gettid);
}

int GetFiberID() {
    return 0;
}
}
