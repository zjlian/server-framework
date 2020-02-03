#ifndef SERVER_FRAMEWORK_CONFIG_H
#define SERVER_FRAMEWORK_CONFIG_H

#include "log.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <sstream>

namespace zjl {

// 配置器的配置项基类
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string& name, const std::string& description)
        : m_name(name), m_description(description) {}
    virtual ~ConfigVarBase() = default;

    const std::string& getName() const { return m_name; }
    const std::string& getDesccription() const { return m_description; }
    // 将相的配置项的值转为为字符串
    virtual std::string toString() = 0;
    // 通过字符串来获设置配置项的值
    virtual bool fromString(const std::string& val) = 0;

protected:
    std::string m_name;        // 配置项的名称
    std::string m_description; // 配置项的备注
};

// 通用型配置项的模板类
template <class T>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;

    ConfigVar(const std::string& name, const T& value, const std::string& description)
        : ConfigVarBase(name, description), m_val(value) {}

    std::string toString() override {
        try {
            // 调用 boost::lexical_cast 类型转换器, 失败抛出 bad_lexical_cast 异常
            return boost::lexical_cast<std::string>(m_val);
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: %s to string",
                          e.what(),
                          typeid(m_val).name());
        }
        return "<error>";
    }

    bool fromString(const std::string& val) override {
        try {
            // 调用 boost::lexical_cast 类型转换器, 失败抛出 bad_lexical_cast 异常
            m_val = boost::lexical_cast<T>(val);
            return true;
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: string to %s",
                          e.what(),
                          typeid(m_val).name());
        }
        return false;
    }

private:
    T m_val; // 配置项的值
};

class Config {
    // TODO
};
}

#endif
