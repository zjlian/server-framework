#include "src/config.h"
#include "src/log.h"
#include "src/util.h"
#include <boost/array.hpp>
#include <iostream>
#include <pthread.h>

void TEST_defaultLogger() {
    std::cout << ">>>>>> Call TEST_defaultLogger 测试日志器的默认用法 <<<<<<" << std::endl;
    auto logger = zjl::LoggerManager::GetInstance()->getLogger("global");
    logger->addAppender(std::make_shared<zjl::StdoutLogAppender>());
    logger->addAppender(std::make_shared<zjl::FileLogAppender>("/home/log/test_all_log", zjl::LogLevel::DEBUG));
    logger->addAppender(std::make_shared<zjl::FileLogAppender>("/home/log/test_error_log", zjl::LogLevel::ERROR));
    std::cout << ">>>>>> 日志将被输出到文件 /home/log/test_all_log 与 /home/log/test_error_log 中 <<<<<<" << std::endl;
    auto event = std::make_shared<zjl::LogEvent>(__FILE__, __LINE__,
                                                 zjl::getThreadID(), zjl::getFiberID(), time(nullptr), "wdnmd");
    logger->log(event);
    logger->debug(event);
    logger->info(event);
    logger->warn(event);
    logger->error(event);
    logger->fatal(event);
}

void TEST_macroLogger() {
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

int main() {
    TEST_defaultLogger();
    TEST_macroLogger();
    TEST_getNonexistentLogger();
    return 0;
}
