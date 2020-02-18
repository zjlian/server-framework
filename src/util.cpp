#include "util.h"

namespace zjl {

inline int64_t getThreadID() {
    return syscall(SYS_gettid);
}

inline int64_t getFiberID() {
    return 0;
}
}
