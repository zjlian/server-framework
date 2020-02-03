//
// Created by zjlian on 2020/2/1.
//

#include "util.h"

namespace zjl {

int64_t getThreadID() {
    return syscall(SYS_gettid);
}

int64_t getFiberID() {
    return 0;
}



}
