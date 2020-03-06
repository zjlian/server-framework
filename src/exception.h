#ifndef SERVER_FRAMEWORK_EXCEPTION_H
#define SERVER_FRAMEWORK_EXCEPTION_H

#include <exception>
#include <string>

namespace zjl
{

/**
 * @brief std::exception 的封装
 * 增加了调用栈信息的获取接口
*/
class Exception : public std::exception
{
public:
    Exception(std::string what);
    ~Exception() noexcept override = default;

    // 获取异常信息
    const char* what() const noexcept override;
    // 获取函数调用栈
    const char* stackTrace() const noexcept;

private:
    std::string m_message;
    std::string m_stack;
};
} // namespace zjl

#endif
