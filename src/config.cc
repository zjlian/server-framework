#include "config.h"

namespace zjl
{

std::ostream& operator<<(std::ostream& out, const ConfigVarBase& cvb)
{
    out << cvb.getName() << ": " << cvb.toString();
    return out;
}
}
