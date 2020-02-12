#include "src/config.h"
#include "src/log.h"
#include "yaml-cpp/yaml.h"
#include <cstdint>
#include <iostream>

// 创建默认配置项
auto config_system_port = zjl::Config::Lookup<uint16_t>("system.port", 6666);
auto config_logger_list = zjl::Config::Lookup<std::vector<std::string>>("logs", {"sb", "sb"});

// 测试通过解析 yaml 文件更新配置项
void TEST_loadConfig(const std::string& path) {
    YAML::Node config;
    try {
        config = YAML::LoadFile(path);
    } catch (const std::exception& e) {
        LOG_FMT_ERROR(GET_ROOT_LOGGER(), "文件加载失败：%s", e.what());
    }
    zjl::Config::LoadFromYAML(config);
}

// 测试获取默认的配置项
void TEST_defaultConfig() {
    std::cout << config_system_port.get() << std::endl;
    std::cout << config_logger_list.get() << std::endl;
}

// 测试获取不存在的配置项
void TEST_nonexistentConfig() {
    auto log_name = zjl::Config::Lookup("nonexistent");
    if (!log_name) {
        LOG_ERROR(GET_ROOT_LOGGER(), "non value");
    }
}

int main() {
    TEST_defaultConfig();
    TEST_loadConfig("/home/workspace/c/server-framework/config.yml");
    TEST_defaultConfig();
    TEST_nonexistentConfig();

    return 0;
}
