#include "src/config.h"
#include "src/log.h"
#include "src/util.h"
#include <boost/array.hpp>
#include <iostream>
#include <pthread.h>
int main() {
    // auto logger = zjl::LoggerManager::GetInstance()->getLogger("global");
    // logger->addAppender(std::make_shared<zjl::StdoutLogAppender>());
    //    logger->addAppender(std::make_shared<zjl::FileLogAppender>("/home/log/test_all_log", zjl::LogLevel::DEBUG));
    //    logger->addAppender(std::make_shared<zjl::FileLogAppender>("/home/log/test_error_log", zjl::LogLevel::ERROR));
    //    auto event = std::make_shared<zjl::LogEvent>(__FILE__, __LINE__,
    //        zjl::getThreadID(), zjl::getFiberID(), time(nullptr), "wdnmd");
    //    logger->log(event);
    //    logger->debug(event);
    //    logger->info(event);
    //    logger->warn(event);
    //    logger->error(event);
    //    logger->fatal(event);
    //    LOG_DEBUG(logger, "消息消息 " + std::to_string(time(nullptr)));
    //    LOG_INFO(logger, "消息消息 " + std::to_string(time(nullptr)));
    //    LOG_WARN(logger, "消息消息 " + std::to_string(time(nullptr)));
    //    LOG_ERROR(logger, "消息消息 " + std::to_string(time(nullptr)));
    //    LOG_FATAL(logger, "消息消息 " + std::to_string(time(nullptr)));
    auto logger = GET_ROOT_LOGGER();
    LOG_FMT_DEBUG(logger, "消息消息 %s", "debug");
    LOG_FMT_INFO(logger, "消息消息 %s", "info");
    LOG_FMT_WARN(logger, "消息消息 %s", "warn");
    LOG_FMT_ERROR(logger, "消息消息 %s", "error");
    LOG_FMT_FATAL(logger, "消息消息 %s", "fatal");

    boost::array<int, 4> arr = {{1, 2, 3, 4}};
    std::cout << arr[0] << std::endl;

    return 0;
}
