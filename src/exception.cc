#include "exception.h"
#include "util.h"

namespace zjl
{
/**
 * =======================================
 * Exception 的实现
 * =======================================
*/

Exception::Exception(std::string msg)
    : m_message(std::move(msg)),
      m_stack(BacktraceToSring(200))
{
}

const char* Exception::what() const noexcept
{
    return m_message.c_str();
}

const char* Exception::stackTrace() const noexcept
{
    return m_stack.c_str();
}

} // namespace zjl
