#include "src/config.h"
#include "src/log.h"
#include "yaml-cpp/yaml.h"
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <vector>

// 创建默认配置项
auto config_system_port = zjl::Config::Lookup<int>("system.port", 6666);
auto config_test_list = zjl::Config::Lookup<std::vector<std::string>>(
    "test_list", std::vector<std::string>{"vector", "string"});
auto config_test_linklist = zjl::Config::Lookup<std::list<std::string>>(
    "test_linklist", std::list<std::string>{"list", "string"});
auto config_test_map = zjl::Config::Lookup<std::map<std::string, std::string>>(
    "test_map", std::map<std::string, std::string>{
                    std::make_pair("map1", "srting"),
                    std::make_pair("map2", "srting"),
                    std::make_pair("map3", "srting")});
auto config_test_set = zjl::Config::Lookup<std::set<int>>(
    "test_set", std::set<int>{10, 20, 30});

// 测试通过解析 yaml 文件更新配置项
void TEST_loadConfig(const std::string& path) {
    LOG_DEBUG(GET_ROOT_LOGGER(), "call TEST_loadConfig 测试通过解析 yaml 文件更新配置项");
    YAML::Node config;
    try {
        config = YAML::LoadFile(path);
    } catch (const std::exception& e) {
        LOG_FMT_ERROR(GET_ROOT_LOGGER(), "文件加载失败：%s", e.what());
    }
    zjl::Config::LoadFromYAML(config);
}

// 测试配置项的 toString 方法
void TEST_ConfigVarToString() {
    LOG_DEBUG(GET_ROOT_LOGGER(), "call TEST_defaultConfig 测试获取默认的配置项");
    std::cout << *config_system_port << std::endl;
    std::cout << *config_test_list << std::endl;
    std::cout << *config_test_linklist << std::endl;
    std::cout << *config_test_map << std::endl;
    std::cout << *config_test_set << std::endl;
}

// 测试获取并使用配置的值
void TEST_GetConfigVarValue() {
    LOG_DEBUG(GET_ROOT_LOGGER(), "call TEST_GetConfigVarValue 测试获取并使用配置的值");
// 遍历线性容器的宏
#define TSEQ(config_var)                                             \
    std::cout << "name = " << config_var->getName() << "; value = "; \
    for (const auto& item : config_var->getValue()) {                \
        std::cout << item << ", ";                                   \
    }                                                                \
    std::cout << std::endl;

    TSEQ(config_test_list);
    TSEQ(config_test_linklist);
    TSEQ(config_test_set);
#undef TSEQ
// 遍历映射容器的宏
#define TMAP(config_var)                                                \
    std::cout << "name = " << config_var->getName() << "; value = ";    \
    for (const auto& pair : config_var->getValue()) {                   \
        std::cout << "<" << pair.first << ", " << pair.second << ">, "; \
    }                                                                   \
    std::cout << std::endl;

    TMAP(config_test_map);
#undef TMAP
}

// 测试获取不存在的配置项
void TEST_nonexistentConfig() {
    LOG_DEBUG(GET_ROOT_LOGGER(), "call TEST_nonexistentConfig 测试获取不存在的配置项");
    auto log_name = zjl::Config::Lookup("nonexistent");
    if (!log_name) {
        LOG_ERROR(GET_ROOT_LOGGER(), "non value");
    }
}

int main() {
    TEST_ConfigVarToString();
    TEST_GetConfigVarValue();
    TEST_loadConfig("/home/workspace/c/server-framework/config.yml");
    TEST_ConfigVarToString();
    TEST_GetConfigVarValue();
    TEST_nonexistentConfig();

    return 0;
}
