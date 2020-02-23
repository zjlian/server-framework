#include "config.h"

namespace zjl {

// Config::ConfigVarMap Config::s_data;

std::ostream& operator<<(std::ostream& out, const ConfigVarBase& cvb) {
    out << cvb.getName() << ": " << cvb.toString();
    return out;
}
}
