#ifndef SERVER_FRAMEWORK_CONFIG_H
#define SERVER_FRAMEWORK_CONFIG_H

#include "log.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace zjl {

// 配置器的配置项基类
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string& name, const std::string& description)
        : m_name(name), m_description(description) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }
    virtual ~ConfigVarBase() = default;

    const std::string& getName() const { return m_name; }
    const std::string& getDesccription() const { return m_description; }
    // 将相的配置项的值转为为字符串
    virtual std::string toString() const = 0;
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
    // 返回配置项的值的字符串
    std::string
    toString() const override {
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

    bool
    fromString(const std::string& val) override {
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
    static ConfigVarBase::ptr
    Lookup(const std::string& name) {
        auto itor = s_data.find(name);
        if (itor == s_data.end()) {
            return nullptr;
        }
        return itor->second;
    }

    // 查找配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name) {
        /* 从 map 取出的配置项对象的类型为基类 ConfigVarBase，
         * 但我们需要使用其派生类的成员函数和变量，
         * 所以需要进行一次明确类型的显示转换
         */
        return std::dynamic_pointer_cast<ConfigVar<T>>(Lookup(name));
    }

    // 检查并创建配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name, const T& value, const std::string& description = "") {
        auto tmp = Lookup<T>(name);
        // 已存在同名配置项
        if (tmp) {
            LOG_FMT_INFO(GET_ROOT_LOGGER(),
                         "Config::Lookup name=%s 已存在",
                         name.c_str());
            return tmp;
        }
        // 判断名称是否合法
        if (name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm0123456789._") != std::string::npos) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "Congif::Lookup exception name=%s"
                          "参数只能以字母数字点或下划线开头",
                          name.c_str());
            throw std::invalid_argument(name);
        }
        auto v = std::make_shared<ConfigVar<T>>(name, value, description);
        s_data[name] = v;
        return v;
    }

    // 从 YAML::Node 中载入配置
    static void
    LoadFromYAML(const YAML::Node& root) {
        std::vector<std::pair<std::string, YAML::Node>> node_list;
        TraversalNode(root, "", node_list);
        // 遍历结果，更新 s_data
        for (const auto& item : node_list) {
            auto itor = s_data.find(item.first);
            if (itor != s_data.end()) {
                // 已存在同名配置项这覆盖当
                s_data[item.first]->fromString(item.second.as<std::string>());
            } else {
                // 创建配置项
                // TODO 创建不同类型的配置项
                Lookup<std::string>(
                    item.first,
                    item.second.as<std::string>());
            }
        }
    }

private:
    // 遍历 YAML::Node 对象，并将遍历结果扁平化存到列表里返回
    static void
    TraversalNode(const YAML::Node& node, const std::string& name,
                  std::vector<std::pair<std::string, YAML::Node>>& output) {
        // 当 YAML::Node 为普通值节点，将结果存入 output
        if (node.IsScalar()) {
            auto itor = std::find_if(
                output.begin(),
                output.end(),
                [&name](const std::pair<std::string, YAML::Node> item) {
                    return item.first == name;
                });
            if (itor != output.end()) {
                itor->second = node;
            } else {
                output.push_back(std::make_pair(name, node));
            }
        }
        // 当 YAML::Node 为映射型节点，使用迭代器遍历
        if (node.IsMap()) {
            for (auto itor = node.begin(); itor != node.end(); ++itor) {
                TraversalNode(
                    itor->second,
                    name.empty() ? itor->first.Scalar()
                                 : name + "." + itor->first.Scalar(),
                    output);
            }
        }
        // 当 YAML::Node 为序列型节点，使用下标遍历
        if (node.IsSequence()) {
            for (size_t i = 0; i < node.size(); ++i) {
                TraversalNode(node[i], name + "." + std::to_string(i), output);
            }
        }
    }

private:
    static ConfigVarMap s_data;
};

/* util functional */
std::ostream& operator<<(std::ostream& out, const ConfigVarBase::ptr cvb);
}

#endif
