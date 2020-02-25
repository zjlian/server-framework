#include "src/config.h"
#include "src/log.h"
#include "src/util.h"
#include <boost/array.hpp>
#include <iostream>
#include <pthread.h>

void TEST_defaultLogger() {
    std::cout << ">>>>>> Call TEST_defaultLogger 测试日志器的默认用法 <<<<<<" << std::endl;
    auto logger = zjl::LoggerManager::GetInstance()->getLogger("global");
    auto event = std::make_shared<zjl::LogEvent>(__FILE__, __LINE__,
                                                 zjl::GetThreadID(), zjl::GetFiberID(), time(nullptr), "wdnmd");
    logger->log(event);
    logger->debug(event);
    logger->info(event);
    logger->warn(event);
    logger->error(event);
    logger->fatal(event);
}

void TEST_macroDefaultLogger() {
    std::cout << ">>>>>> Call TEST_macroLogger 测试日志器的宏函数 <<<<<<" << std::endl;
    auto logger = GET_ROOT_LOGGER();
    LOG_DEBUG(logger, "消息消息 " + std::to_string(time(nullptr)));
    LOG_INFO(logger, "消息消息 " + std::to_string(time(nullptr)));
    LOG_WARN(logger, "消息消息 " + std::to_string(time(nullptr)));
    LOG_ERROR(logger, "消息消息 " + std::to_string(time(nullptr)));
    LOG_FATAL(logger, "消息消息 " + std::to_string(time(nullptr)));
    LOG_FMT_DEBUG(logger, "消息消息 %s", "debug");
    LOG_FMT_INFO(logger, "消息消息 %s", "info");
    LOG_FMT_WARN(logger, "消息消息 %s", "warn");
    LOG_FMT_ERROR(logger, "消息消息 %s", "error");
    LOG_FMT_FATAL(logger, "消息消息 %s", "fatal");
}

void TEST_getNonexistentLogger() {
    std::cout << ">>>>>> Call TEST_getNonexistentLogger 测试获取不存在的日志器 <<<<<<" << std::endl;
    try {
        zjl::LoggerManager::GetInstance()->getLogger("nonexistent");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}

void TEST_loggerConfig() {
    std::cout << ">>>>>> Call TEST_loggerConfig 测试日志器的配置文件加载 <<<<<<" << std::endl;
    auto config = zjl::Config::Lookup("logs");
    LOG_DEBUG(GET_ROOT_LOGGER(), config->toString().c_str());
    auto yaml_node = YAML::LoadFile("/home/workspace/c/server-framework/config.yml");
    zjl::Config::LoadFromYAML(yaml_node);
    LOG_DEBUG(GET_ROOT_LOGGER(), config->toString().c_str());
}

void TEST_createLoggerByYAMLFile() {
    std::cout << ">>>>>> Call TEST_createAndUsedLogger 测试配置功能 <<<<<<" << std::endl;
    auto yaml_node = YAML::LoadFile("/home/workspace/c/server-framework/config.yml");
    zjl::Config::LoadFromYAML(yaml_node);
    auto global_logger = zjl::LoggerManager::GetInstance()->getGlobal();
    auto system_logger = zjl::LoggerManager::GetInstance()->getLogger("system");

    LOG_DEBUG(global_logger, "输出一条 debug 日志到全局日志器");
    LOG_INFO(global_logger, "输出一条 info 日志到全局日志器");
    LOG_ERROR(global_logger, "输出一条 error 日志到全局日志器");

    LOG_DEBUG(system_logger, "输出一条 debug 日志到 system 日志器");
    LOG_INFO(system_logger, "输出一条 info 日志到 system 日志器");
    LOG_ERROR(system_logger, "输出一条 error 日志到 system 日志器");
    // auto event = MAKE_LOG_EVENT(zjl::LogLevel::DEBUG, "输出一条 debug 日志");
    // global_logger->log(event);
    // system_logger->log(event);
}

int main() {
    TEST_macroDefaultLogger();
    // TEST_defaultLogger();
    // TEST_macroDefaultLogger();
    // TEST_getNonexistentLogger();
    // TEST_createAndUsedLogger();
    // TEST_loggerConfig();
    TEST_createLoggerByYAMLFile();

    return 0;
}
