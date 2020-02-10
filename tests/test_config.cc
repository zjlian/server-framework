#include "src/config.h"
#include "src/log.h"
#include "yaml-cpp/yaml.h"
#include <iostream>

void loadConfig(const YAML::Node& root_node) {
#define RANGE(OBJ) OBJ.begin(), OBJ.end()
    auto logs_node = root_node["logs"];
    if (!logs_node.IsNull()) {
        std::for_each(RANGE(logs_node), [](const YAML::Node& log_item) {
            std::cout << log_item["name"] << std::endl;
            std::cout << log_item["level"] << std::endl;
            std::cout << log_item["formatter"] << std::endl;
            // std::cout << log_item["appender"] << std::endl;
            auto appender_list = log_item["appender"];
            std::for_each(RANGE(appender_list), [](const YAML::Node& appender) {
                std::cout << appender["type"] << std::endl;
            });
        });
    }
#undef RANGE
}

int main() {
    auto int_value_config = zjl::Config::Lookup("config", 55555);
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%s", typeid(int_value_config).name());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", int_value_config->getValue());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", zjl::Config::Lookup<int>("config")->getValue());

    zjl::Config::Lookup("system.port", 6666);
    auto port = zjl::Config::Lookup("system.port");
    std::cout << port << std::endl;

    YAML::Node config;
    try {
        config = YAML::LoadFile("/home/workspace/c/server-framework/config.yml");
    } catch (const std::exception& e) {
        LOG_FMT_ERROR(GET_ROOT_LOGGER(), "文件加载失败：%s", e.what());
    }
    zjl::Config::LoadFromYAML(config);
    std::cout << port << std::endl;

    auto log_name = zjl::Config::Lookup("logs.0.name");
    if (log_name) {
        LOG_DEBUG(GET_ROOT_LOGGER(), "has value");
        std::cout << log_name << std::endl;
    } else {
        LOG_ERROR(GET_ROOT_LOGGER(), "non value");
    }

    // std::cout << zjl::Config::Lookup("logs.0.name")->toString() << std::endl;
    // std::cout << zjl::Config::Lookup("logs.0.level")->toString() << std::endl;
    // std::cout << zjl::Config::Lookup("logs.0.formatter")->toString() << std::endl;
    // std::cout << zjl::Config::Lookup("logs.0.formatter.0.type")->toString() << std::endl;
    // std::cout << zjl::Config::Lookup("logs.0.formatter.0.file")->toString() << std::endl;

    return 0;
}
