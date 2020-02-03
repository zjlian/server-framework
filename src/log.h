//
// Created by zjlian on 2020/1/27.
//

#ifndef SERVER_FRAMEWORK_LOG_H
#define SERVER_FRAMEWORK_LOG_H

#include "util.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <strstream>
#include <vector>

#define LOG_LEVEL(logger, level, massage) \
    logger->log(std::make_shared<zjl::LogEvent>(__FILE__, __LINE__, zjl::getThreadID(), zjl::getFiberID(), time(nullptr), massage, level));

#define LOG_DEBUG(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::DEBUG, massage)
#define LOG_INFO(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::INFO, massage)
#define LOG_WARN(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::WARN, massage)
#define LOG_ERROR(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::ERROR, massage)
#define LOG_FATAL(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::FATAL, massage)

#define LOG_FMT_LEVEL(logger, level, format, argv...)    \
    {                                                    \
        char *b = nullptr;                               \
        int l = asprintf(&b, format, argv);              \
        if (l != -1) {                                   \
            LOG_LEVEL(logger, level, std::string(b, l)); \
            free(b);                                     \
        }                                                \
    }

#define LOG_FMT_DEBUG(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::DEBUG, format, argv)
#define LOG_FMT_INFO(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::INFO, format, argv)
#define LOG_FMT_WARN(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::WARN, format, argv)
#define LOG_FMT_ERROR(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::ERROR, format, argv)
#define LOG_FMT_FATAL(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::FATAL, format, argv)

namespace zjl {
// 日志级别
class LogLevel {
public:
    enum Level {
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static std::string levelToString(LogLevel::Level level);
};

// 日志信息
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(const std::string &filename, uint32_t line, uint32_t thread_id,
             uint32_t fiber_id, time_t time, const std::string &content, LogLevel::Level level = LogLevel::DEBUG)
        : m_level(level), m_filename(filename), m_line(line), m_thread_id(thread_id),
          m_fiber_id(fiber_id), m_time(time), m_content(content) {}

    const std::string &getFilename() const { return m_filename; }
    LogLevel::Level getLevel() const { return m_level; }
    uint32_t getLine() const { return m_line; }
    uint32_t getThreadId() const { return m_thread_id; }
    uint32_t getFiberId() const { return m_fiber_id; }
    time_t getTime() const { return m_time; }
    const std::string &getContent() const { return m_content; }

    void setLevel(LogLevel::Level level) { m_level = level; }

private:
    LogLevel::Level m_level;  //日志等级
    std::string m_filename;   // 文件名
    uint32_t m_line = 0;      // 行号
    uint32_t m_thread_id = 0; // 线程号
    uint32_t m_fiber_id = 0;  // 协程号
                              //    uint32_t m_elapse = 0;        // 程序启动到现在的时间
    time_t m_time;            // 时间
    std::string m_content;
};

//日志格式化器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;

        virtual void format(std::ostream &out, LogEvent::ptr ev) = 0;
    };

    explicit LogFormatter(const std::string &pattern = "");
    std::string format(LogEvent::ptr ev);

private:
    void init();

    std::string m_format_pattern;                    // 日志格式化字符串
    std::vector<FormatItem::ptr> m_format_item_list; // 格式化字符串解析后的解析器列表
};

// 日志输出器基类
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;

    explicit LogAppender(LogLevel::Level level = LogLevel::DEBUG);
    virtual ~LogAppender() = default;
    // 纯虚函数，让派生类来实现
    virtual void log(LogLevel::Level level, LogEvent::ptr ev) = 0;

    LogFormatter::ptr getFormatter() const { return m_formatter; }
    void setFormatter(const LogFormatter::ptr &formatter) { m_formatter = formatter; }

protected:
    LogLevel::Level m_level;       // 输出器的日志等级
    LogFormatter::ptr m_formatter; // 格式化器，将LogEvent对象格式化为指定的字符串格式
};

// 日志器
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    explicit Logger(const std::string &name = "root", const std::string &pattern = "[%d] [%p] [%f:%l@%t:%F]%T%m%n");
    virtual void log(LogEvent::ptr ev);
    void debug(LogEvent::ptr ev);
    void info(LogEvent::ptr ev);
    void warn(LogEvent::ptr ev);
    void error(LogEvent::ptr ev);
    void fatal(LogEvent::ptr ev);
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);

    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level level) { m_level = level; }

private:
    const std::string m_name;                    // 日志名称
    LogLevel::Level m_level;                     // 日志有效级别
    std::string m_format_pattern;                // 日志输格式化器的默认pattern
    LogFormatter::ptr m_formatter;               // 日志默认格式化器
    std::list<LogAppender::ptr> m_appender_list; // Appender列表
};

//输出到终端的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    explicit StdoutLogAppender(LogLevel::Level level = LogLevel::DEBUG);
    void log(LogLevel::Level level, LogEvent::ptr ev) override;
};

//输出到文件的Appender
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    explicit FileLogAppender(const std::string &filename, LogLevel::Level level = LogLevel::DEBUG);
    void log(LogLevel::Level level, LogEvent::ptr ev) override;
    bool reopen();

private:
    std::string m_filename;
    std::ofstream m_file_stream;
};

// 日志器的管理器
class __LoggerManager {
public:
    typedef std::shared_ptr<__LoggerManager> ptr;
    __LoggerManager();

    Logger::ptr getLogger(const std::string &name);

private:
    void init();
    std::map<std::string, Logger::ptr> m_logger_map;
};

typedef SingletonPtr<__LoggerManager> LoggerManager;
}
#endif //SERVER_FRAMEWORK_LOG_H
