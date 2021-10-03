#ifndef SERVER_FRAMEWORK_EXCEPTION_H
#define SERVER_FRAMEWORK_EXCEPTION_H

#include <exception>
#include <string>
#include <cstring>
#include <cerrno>

#define THROW_EXCEPTION_WHIT_ERRNO                       \
    do                                                   \
    {                                                    \
        throw Exception(std::string(::strerror(errno))); \
    } while (0)

namespace zjl
{

/**
 * @brief std::exception 的封装
 * 增加了调用栈信息的获取接口
*/
class Exception : public std::exception
{
public:
    explicit Exception(std::string what);
    ~Exception() noexcept override = default;

    // 获取异常信息
    const char* what() const noexcept override;
    // 获取函数调用栈
    const char* stackTrace() const noexcept;

protected:
    std::string m_message;
    std::string m_stack;
};

class SystemError : public Exception
{
public:
    explicit SystemError(std::string what = "");
};


} // namespace zjl

#endif
