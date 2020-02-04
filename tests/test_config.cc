#include "src/config.h"
#include "src/log.h"
#include "yaml-cpp/yaml.h"

int main() {
    auto int_value_config = zjl::Config::Lookup("config", 55555);
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%s", typeid(int_value_config).name());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", int_value_config->getValue());
    LOG_FMT_DEBUG(GET_ROOT_LOGGER(), "%d", zjl::Config::Lookup<int>("config")->getValue());



    return 0;
}
