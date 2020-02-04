#include "src/config.h"
#include "src/log.h"
#include "yaml-cpp/yaml.h"
#include <iostream>

int main() {
    auto int_value_config = zjl::Config::Lookup("config", 55555);
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%s", typeid(int_value_config).name());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", int_value_config->getValue());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", zjl::Config::Lookup<int>("config")->getValue());

    YAML::Node config;
    try {
        config = YAML::LoadFile("../config.yml");
    } catch (const std::exception& e) {
        LOG_FMT_ERROR(GET_ROOT_LOGGER(), "文件加载失败：%s", e.what());
    }

    std::cout << config;

    return 0;
}
