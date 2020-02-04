#ifndef SERVER_FRAMEWORK_CONFIG_H
#define SERVER_FRAMEWORK_CONFIG_H

#include "log.h"
#include <boost/lexical_cast.hpp>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

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
        : ConfigVarBase(name, description), m_value(value) {}

    const T getValue() const { return m_value; }
    void setValue(const T value) { m_value = value; }
    std::string toString() override {
        try {
            // 调用 boost::lexical_cast 类型转换器, 失败抛出 bad_lexical_cast 异常
            return boost::lexical_cast<std::string>(m_value);
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: %s to string",
                          e.what(),
                          typeid(m_value).name());
        }
        return "<error>";
    }

    bool fromString(const std::string& val) override {
        try {
            // 调用 boost::lexical_cast 类型转换器, 失败抛出 bad_lexical_cast 异常
            m_value = boost::lexical_cast<T>(val);
            return true;
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: string to %s",
                          e.what(),
                          typeid(m_value).name());
        }
        return false;
    }

private:
    T m_value; // 配置项的值
};

class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    // 查找配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name) {
        auto itor = s_data.find(name);
        if (itor == s_data.end()) {
            return nullptr;
        }
        /* 从 map 取出的配置项对象的类型为基类 ConfigVarBase，
         * 但我们需要使用其派生类的成员函数和变量，
         * 所以需要进行一次显示的类型转换
         */
        return std::dynamic_pointer_cast<ConfigVar<T>>(itor->second);
    }

    // 检查并创建配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name, const T& value, const std::string& description = "") {
        auto tmp = Lookup<T>(name);
        // 已存在同名配置项
        if (tmp) {
            LOG_FMT_INFO(GET_ROOT_LOGGER(), "Config::Lookup name=%s 已存在", name.c_str());
            return tmp;
        }
        // 判断名称是否合法
        if (name.find_first_not_of(
                "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm0123456789._") != std::string::npos) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "Congif::Lookup exception name=%s 参数只能以字母数字点或下划线开头",
                          name.c_str());
            throw std::invalid_argument(name);
        }
        auto v = std::make_shared<ConfigVar<T>>(name, value, description);
        s_data[name] = v;
        return v;
    }

private:
    static ConfigVarMap s_data;
};
}

#endif
