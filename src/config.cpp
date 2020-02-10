#include "config.h"

namespace zjl {

Config::ConfigVarMap Config::s_data;

std::ostream& operator<<(std::ostream& out, const ConfigVarBase::ptr cvb) {
    if (cvb) {
        out << cvb->getName() << ": " << cvb->toString();
    } else {
        out << "<empty shared ptr>";
    }
    return out;
}
}
