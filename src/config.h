#ifndef SERVER_FRAMEWORK_CONFIG_H
#define SERVER_FRAMEWORK_CONFIG_H

// #include "log.h"
#include "thread.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace zjl
{

// @brief 配置项基类
class ConfigVarBase
{
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string& name, const std::string& description)
        : m_name(name), m_description(description)
    {
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

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * boost::lexical_cast 的包装，
 * 因为 boost::lexical_cast 是使用 std::stringstream 实现的类型转换，
 * 所以仅支持实现了 ostream::operator<< 与 istream::operator>> 的类型,
 * 可以说默认情况下仅支持 std::string 与各类 Number 类型的双向转换。
 * 需要转换自定义的类型，可以选择实现对应类型的流操作符，或者将该模板类进行偏特化
*/
template <typename Source, typename Target>
class LexicalCast
{
public:
    Target operator()(const Source& source)
    {
        return boost::lexical_cast<Target>(source);
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::vector<T> 的转换，
 * 接受可被 YAML::Load() 解析的字符串
*/
template <typename T>
class LexicalCast<std::string, std::vector<T>>
{
public:
    std::vector<T> operator()(const std::string& source)
    {
        YAML::Node node;
        // 调用 YAML::Load 解析传入的字符串，解析失败会抛出异常
        node = YAML::Load(source);
        std::vector<T> config_list;
        // 检查解析后的 node 是否是一个序列型 YAML::Node
        if (node.IsSequence())
        {
            std::stringstream ss;
            for (const auto& item : node)
            {
                ss.str("");
                // 利用 YAML::Node 实现的 operator<<() 将 node 转换为字符串
                ss << item;
                // 递归解析，直到 T 为基本类型
                config_list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
        }
        else
        {
            // LOG_FMT_INFO(
            //     GET_ROOT_LOGGER(),
            //     "LexicalCast<std::string, std::vector>::operator() exception %s",
            //     "<source> is not a YAML sequence");
        }
        return config_list;
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::list<T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::vector<T>, std::string>
{
public:
    std::string operator()(const std::vector<T>& source)
    {
        YAML::Node node;
        // 暴力解析，将 T 解析成字符串，在解析回 YAML::Node 插入 node 的尾部，
        // 最后通过 std::stringstream 与调用 yaml-cpp 库实现的 operator<<() 将 node 转换为字符串
        for (const auto& item : source)
        {
            // 调用 LexicalCast 递归解析，知道 T 为基本类型
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::list<T> 的转换，
*/
template <typename T>
class LexicalCast<std::string, std::list<T>>
{
public:
    std::list<T> operator()(const std::string& source)
    {
        YAML::Node node;
        node = YAML::Load(source);
        std::list<T> config_list;
        if (node.IsSequence())
        {
            std::stringstream ss;
            for (const auto& item : node)
            {
                ss.str("");
                ss << item;
                config_list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
        }
        else
        {
            // LOG_FMT_INFO(
            //     GET_ROOT_LOGGER(),
            //     "LexicalCast<std::string, std::list>::operator() exception %s",
            //     "<source> is not a YAML sequence");
        }
        return config_list;
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::list<T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::list<T>, std::string>
{
public:
    std::string operator()(const std::list<T>& source)
    {
        YAML::Node node;
        for (const auto& item : source)
        {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::map<std::string, T> 的转换，
*/
template <typename T>
class LexicalCast<std::string, std::map<std::string, T>>
{
public:
    std::map<std::string, T> operator()(const std::string& source)
    {
        YAML::Node node;
        node = YAML::Load(source);
        std::map<std::string, T> config_map;
        if (node.IsMap())
        {
            std::stringstream ss;
            for (const auto& item : node)
            {
                ss.str("");
                ss << item.second;
                config_map.insert(std::make_pair(
                    item.first.as<std::string>(),
                    LexicalCast<std::string, T>()(ss.str())));
            }
        }
        else
        {
            // LOG_FMT_INFO(
            //     GET_ROOT_LOGGER(),
            //     "LexicalCast<std::string, std::map>::operator() exception %s",
            //     "<source> is not a YAML map");
        }
        return config_map;
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::map<std::string, T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::map<std::string, T>, std::string>
{
public:
    std::string operator()(const std::map<std::string, T>& source)
    {
        YAML::Node node;
        for (const auto& item : source)
        {
            node[item.first] = YAML::Load(LexicalCast<T, std::string>()(item.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::set<T> 的转换，
*/
template <typename T>
class LexicalCast<std::string, std::set<T>>
{
public:
    std::set<T> operator()(const std::string& source)
    {
        YAML::Node node;
        node = YAML::Load(source);
        std::set<T> config_set;
        if (node.IsSequence())
        {
            std::stringstream ss;
            for (const auto& item : node)
            {
                ss.str("");
                ss << item;
                config_set.insert(LexicalCast<std::string, T>()(ss.str()));
                // config_list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
        }
        else
        {
            // LOG_FMT_INFO(
            //     GET_ROOT_LOGGER(),
            //     "LexicalCast<std::string, std::list>::operator() exception %s",
            //     "<source> is not a YAML sequence");
        }
        return config_set;
    }
};

/**
 * @brief YAML格式字符串到其他类型的转换仿函数
 * LexicalCast 的偏特化，针对 std::set<T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::set<T>, std::string>
{
public:
    std::string operator()(const std::set<T>& source)
    {
        YAML::Node node;
        for (const auto& item : source)
        {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 通用型配置项的模板类
 * 模板参数:
 *      T               配置项的值的类型
 *      ToStringFN      {functor<std::string(T&)>} 将配置项的值转换为 std::string
 *      FromStringFN    {functor<T(const std::string&)>} 将 std::string 转换为配置项的值
 * */
template <
    class T,
    class ToStringFN = LexicalCast<T, std::string>,
    class FromStringFN = LexicalCast<std::string, T>>
class ConfigVar : public ConfigVarBase
{
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    typedef std::function<void(const T& old_value, const T& new_value)> onChangeCallback;

    ConfigVar(const std::string& name, const T& value, const std::string& description)
        : ConfigVarBase(name, description), m_value(value) {}

    // thread-safe 获取配置项的值
    T getValue() const
    {
        ReadScopedLock lock(&m_mutex);
        return m_value;
    }
    // thread-safe 设置配置项的值
    void setValue(const T value)
    {
        { // 上读锁
            ReadScopedLock lock(&m_mutex);
            if (value == m_value)
            {
                return;
            }
            T old_value = m_value;
            // 值被修改，调用所有的变更事件处理器
            for (const auto& pair : m_callback_map)
            {
                pair.second(old_value, m_value);
            }
        }
        // 上写锁
        WriteScopedLock lock(&m_mutex);
        m_value = value;
    }
    // 返回配置项的值的字符串
    std::string toString() const override
    {
        try
        {
            // 默认 ToStringFN 调用了 boost::lexical_cast 进行类型转换, 失败抛出异常 bad_lexical_cast
            return ToStringFN()(getValue());
        }
        catch (std::exception& e)
        {
            // LOG_FMT_ERROR(GET_ROOT_LOGGER(),
            //               "ConfigVar::toString exception %s convert: %s to string",
            //               e.what(),
            //               typeid(m_value).name());
            std::cerr << "ConfigVar::toString exception "
                      << e.what()
                      << " convert: "
                      << typeid(m_value).name()
                      << " to string" << std::endl;
        }
        return "<error>";
    }
    // 将 yaml 文本转换为配置项的值
    bool fromString(const std::string& val) override
    {
        try
        {
            //  默认 FromStringFN 调用了 boost::lexical_cast 进行类型转换, 失败抛出异常 bad_lexical_cast
            setValue(FromStringFN()(val));
            return true;
        }
        catch (std::exception& e)
        {
            // LOG_FMT_ERROR(GET_ROOT_LOGGER(),
            //               "ConfigVar::toString exception %s convert: string to %s",
            //               e.what(),
            //               typeid(m_value).name());
            std::cerr << "ConfigVar::fromString exception "
                      << e.what()
                      << " convert: "
                      << "string to "
                      << typeid(m_value).name() << std::endl;
        }
        return false;
    }

    // thread-safe 增加配置项变更事件处理器，返回处理器的唯一编号
    uint64_t addListener(onChangeCallback cb)
    {
        static uint64_t s_cb_id = 0;
        WriteScopedLock lock(&m_mutex);
        m_callback_map[s_cb_id] = cb;
        return s_cb_id;
    }
    // thread-safe 删除配置项变更事件处理器
    void delListener(uint64_t key)
    {
        WriteScopedLock lock(&m_mutex);
        m_callback_map.erase(key);
    }

    // thread-safe 获取配置项变更事件处理器
    onChangeCallback getListener(uint64_t key)
    {
        ReadScopedLock lock(&m_mutex);
        auto iter = m_callback_map.find(key);
        if (iter == m_callback_map.end())
        {
            return nullptr;
        }
        return iter->second;
    }

    // thread-safe 清除所有配置项变更事件处理器
    void clearListener()
    {
        WriteScopedLock lock(&m_mutex);
        m_callback_map.clear();
    }

private:
    T m_value; // 配置项的值
    std::map<uint64_t, onChangeCallback> m_callback_map;
    mutable RWLock m_mutex;
};

class Config
{
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    // thread-safe 查找配置项，返回 ConfigVarBase 智能指针
    static ConfigVarBase::ptr
    Lookup(const std::string& name)
    {
        ReadScopedLock lock(&GetRWLock());
        ConfigVarMap& s_data = GetData();
        auto iter = s_data.find(name);
        if (iter == s_data.end())
        {
            return nullptr;
        }
        return iter->second;
    }

    // 查找配置项，返回指定类型的 ConfigVar 智能指针
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name)
    {
        auto base_ptr = Lookup(name);
        if (!base_ptr)
        {
            return nullptr;
        }
        // 配置项存在，尝试转换成指定的类型
        auto ptr = std::dynamic_pointer_cast<ConfigVar<T>>(base_ptr);
        // 如果 std::dynamic_pointer_cast 转型失败会返回一个空的智能指针
        // 调用 operator bool() 来判断
        if (!ptr)
        {
            // LOG_ERROR(GET_ROOT_LOGGER(), "Config::Lookup<T> exception, 无法转换 ConfigVar<T> 的实际类型到模板参数类型 T");
            std::cerr << "Config::Lookup<T> exception, 无法转换 ConfigVar<T> 的实际类型到模板参数类型 T" << std::endl;
            throw std::bad_cast();
        }
        return ptr;
    }

    // thread-safe 创建或更新配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name, const T& value, const std::string& description = "")
    {
        auto tmp = Lookup<T>(name);
        // 已存在同名配置项
        if (tmp)
        {
            // LOG_FMT_INFO(GET_ROOT_LOGGER(),
            //              "Config::Lookup name=%s 已存在",
            //              name.c_str());
            return tmp;
        }
        // 判断名称是否合法
        if (name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm0123456789._") != std::string::npos)
        {
            // LOG_FMT_ERROR(GET_ROOT_LOGGER(),
            //               "Congif::Lookup exception name=%s"
            //               "参数只能以字母数字点或下划线开头",
            //               name.c_str());
            std::cerr << "Congif::Lookup exception, 参数只能以字母数字点或下划线开头" << std::endl;
            throw std::invalid_argument(name);
        }
        auto v = std::make_shared<ConfigVar<T>>(name, value, description);
        WriteScopedLock lock(&GetRWLock());
        GetData()[name] = v;
        return v;
    }

    // thread-safe 从 YAML::Node 中载入配置
    static void LoadFromYAML(const YAML::Node& root)
    {
        std::vector<std::pair<std::string, YAML::Node>> node_list;
        TraversalNode(root, "", node_list);

        for (const auto& node : node_list)
        {
            std::string key = node.first;
            if (key.empty())
            {
                continue;
            }
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            // 根据配置项名称获取配置项
            auto var = Lookup(key);
            // 只处理注册过的配置项
            if (var)
            {
                std::stringstream ss;
                ss << node.second;
                var->fromString(ss.str());
            }
        }
    }

private:
    // 遍历 YAML::Node 对象，并将遍历结果扁平化存到列表里返回
    static void
    TraversalNode(const YAML::Node& node, const std::string& name,
                  std::vector<std::pair<std::string, YAML::Node>>& output)
    {
        // 将 YAML::Node 存入 output
        auto output_iter = std::find_if(
            output.begin(),
            output.end(),
            [&name](const std::pair<std::string, YAML::Node>& item) {
                return item.first == name;
            });
        if (output_iter != output.end())
        {
            output_iter->second = node;
        }
        else
        {
            output.push_back(std::make_pair(name, node));
        }
        // 当 YAML::Node 为映射型节点，使用迭代器遍历
        if (node.IsMap())
        {
            for (auto iter = node.begin(); iter != node.end(); ++iter)
            {
                TraversalNode(
                    iter->second,
                    name.empty() ? iter->first.Scalar()
                                 : name + "." + iter->first.Scalar(),
                    output);
            }
        }
        // 当 YAML::Node 为序列型节点，使用下标遍历
        if (node.IsSequence())
        {
            for (size_t i = 0; i < node.size(); ++i)
            {
                TraversalNode(node[i], name + "." + std::to_string(i), output);
            }
        }
    }

private:
    static ConfigVarMap& GetData()
    {
        static ConfigVarMap s_data;
        return s_data;
    }

    static RWLock& GetRWLock()
    {
        static RWLock s_lock;
        return s_lock;
    }
};

/* util functional */
std::ostream& operator<<(std::ostream& out, const ConfigVarBase& cvb);
} // namespace zjl

#endif
