#ifndef SERVER_FRAMEWORK_LOG_H
#define SERVER_FRAMEWORK_LOG_H

#include "config.h"
#include "thread.h"
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

#define MAKE_LOG_EVENT(level, massage) \
    std::make_shared<zjl::LogEvent>(__FILE__, __LINE__, zjl::GetThreadID(), zjl::GetFiberID(), time(nullptr), massage, level)

#define LOG_LEVEL(logger, level, massage) \
    logger->log(MAKE_LOG_EVENT(level, massage));

#define LOG_DEBUG(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::DEBUG, massage)
#define LOG_INFO(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::INFO, massage)
#define LOG_WARN(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::WARN, massage)
#define LOG_ERROR(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::ERROR, massage)
#define LOG_FATAL(logger, massage) LOG_LEVEL(logger, zjl::LogLevel::FATAL, massage)

#define LOG_FMT_LEVEL(logger, level, format, argv...)    \
    {                                                    \
        char* b = nullptr;                               \
        int l = asprintf(&b, format, argv);              \
        if (l != -1)                                     \
        {                                                \
            LOG_LEVEL(logger, level, std::string(b, l)); \
            free(b);                                     \
        }                                                \
    }

#define LOG_FMT_DEBUG(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::DEBUG, format, argv)
#define LOG_FMT_INFO(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::INFO, format, argv)
#define LOG_FMT_WARN(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::WARN, format, argv)
#define LOG_FMT_ERROR(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::ERROR, format, argv)
#define LOG_FMT_FATAL(logger, format, argv...) LOG_FMT_LEVEL(logger, zjl::LogLevel::FATAL, format, argv)

#define GET_ROOT_LOGGER() zjl::LoggerManager::GetInstance()->getGlobal()
#define GET_LOGGER(name) zjl::LoggerManager::GetInstance()->getLogger(name)

namespace zjl
{
// 日志级别
class LogLevel
{
public:
    enum Level
    {
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static std::string levelToString(LogLevel::Level level);
};

/**
 * @brief 日志器的 appender 的配置信息类
*/
struct LogAppenderConfig
{
    enum Type
    {
        Stdout = 0,
        File = 1
    };
    LogAppenderConfig::Type type; // 输出器的类型
    LogLevel::Level level;        // 输出器的日志有效等级
    std::string formatter;        // 输出器的日志打印格式
    std::string file;             // 输出器的目标文件路径

    LogAppenderConfig()
        : type(Type::Stdout), level(LogLevel::UNKNOWN) {}

    bool operator==(const LogAppenderConfig& lhs) const
    {
        return type == lhs.type &&
               level == lhs.level &&
               formatter == lhs.formatter &&
               file == lhs.file;
    }
};

/**
 * @brief 日志器的配置信息类
*/
struct LogConfig
{
    std::string name;                        // 日志器名称
    LogLevel::Level level;                   // 日志器的日志有效等级
    std::string formatter;                   // 日志器的日志打印格式
    std::vector<LogAppenderConfig> appender; // 日志器的输出器配置集合

    LogConfig()
        : level(LogLevel::UNKNOWN) {}

    bool operator==(const LogConfig& lhs) const
    {
        return name == lhs.name /* &&
               level == lhs.level &&
               formatter == lhs.formatter &&
               appender == lhs.appender*/;
    }
};

/**
 * @brief LexicalCast 的偏特化
*/
template <>
class LexicalCast<std::string, std::vector<LogConfig>>
{
public:
    std::vector<LogConfig> operator()(const std::string& source)
    {
        auto node = YAML::Load(source);
        std::vector<LogConfig> result{};
        if (node.IsSequence())
        {
            for (const auto log_config : node)
            {
                LogConfig lc{};
                lc.name = log_config["name"] ? log_config["name"].as<std::string>() : "";
                lc.level = log_config["level"] ? (LogLevel::Level)(log_config["level"].as<int>()) : LogLevel::UNKNOWN;
                lc.formatter = log_config["formatter"] ? log_config["formatter"].as<std::string>() : "";
                if (log_config["appender"] && log_config["appender"].IsSequence())
                {
                    for (const auto app_config : log_config["appender"])
                    {
                        LogAppenderConfig ac{};
                        ac.type = (LogAppenderConfig::Type)(app_config["type"] ? app_config["type"].as<int>() : 0);
                        ac.file = app_config["file"] ? app_config["file"].as<std::string>() : "";
                        ac.level = (LogLevel::Level)(app_config["level"] ? app_config["level"].as<int>() : lc.level);
                        ac.formatter = app_config["formatter"] ? app_config["formatter"].as<std::string>() : lc.formatter;
                        lc.appender.push_back(ac);
                    }
                }
                result.push_back(lc);
            }
        }
        return result;
    }
};

template <>
class LexicalCast<std::vector<LogConfig>, std::string>
{
public:
    std::string operator()(const std::vector<LogConfig>& source)
    {
        YAML::Node node;
        for (const auto log_config : source)
        {
            node["name"] = log_config.name;
            node["level"] = (int)(log_config.level);
            node["formatter"] = log_config.formatter;
            YAML::Node app_list_node;
            for (const auto app_config : log_config.appender)
            {
                YAML::Node app_node;
                app_node["type"] = (int)(app_config.type);
                app_node["file"] = app_config.file;
                app_node["level"] = (int)(app_config.level);
                app_node["formatter"] = app_config.formatter;
                app_list_node.push_back(app_node);
            }
            node["appender"] = app_list_node;
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 日志消息类
*/
class LogEvent
{
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(const std::string& filename,
             uint32_t line,
             uint32_t thread_id,
             uint32_t fiber_id,
             time_t time,
             const std::string& content,
             LogLevel::Level level = LogLevel::DEBUG)
        : m_level(level),
          m_filename(filename),
          m_line(line),
          m_thread_id(thread_id),
          m_fiber_id(fiber_id),
          m_time(time),
          m_content(content) {}

    const std::string& getFilename() const { return m_filename; }
    LogLevel::Level getLevel() const { return m_level; }
    uint32_t getLine() const { return m_line; }
    uint32_t getThreadId() const { return m_thread_id; }
    uint32_t getFiberId() const { return m_fiber_id; }
    time_t getTime() const { return m_time; }
    const std::string& getContent() const { return m_content; }

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

/**
 * @brief 日志格式化器
 * 构造时传入日志格式化规则的字符串，调用 format() 传入 LogEvent 实例，返回格式化后的字符串
*/
class LogFormatter
{
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    class FormatItem
    {
    public:
        typedef std::shared_ptr<FormatItem> ptr;

        virtual void format(std::ostream& out, LogEvent::ptr ev) = 0;
    };

    explicit LogFormatter(const std::string& pattern /* = ""*/);
    std::string format(LogEvent::ptr ev);

private:
    void init();

    std::string m_format_pattern;                    // 日志格式化字符串
    std::vector<FormatItem::ptr> m_format_item_list; // 格式化字符串解析后的解析器列表
};

// 日志输出器基类
class LogAppender
{
public:
    typedef std::shared_ptr<LogAppender> ptr;

    explicit LogAppender(LogLevel::Level level = LogLevel::DEBUG);
    virtual ~LogAppender() = default;
    // 纯虚函数，让派生类来实现
    virtual void log(LogLevel::Level level, LogEvent::ptr ev) = 0;

    // thread-safe 获取格式化器
    LogFormatter::ptr getFormatter();
    // thread-safe 设置格式化器
    void setFormatter(const LogFormatter::ptr formatter);

protected:
    LogLevel::Level m_level;       // 输出器的日志等级
    LogFormatter::ptr m_formatter; // 格式化器，将LogEvent对象格式化为指定的字符串格式
    Mutex m_mutex;
};

// 日志器
class Logger
{
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger();
    Logger(const std::string& name, LogLevel::Level level, const std::string& pattern);
    // thread-safe 输出日志
    void log(LogEvent::ptr ev);
    // TODO 下列注释的方法有待重新设计，或者不需要
    // void debug(LogEvent::ptr ev);
    // void info(LogEvent::ptr ev);
    // void warn(LogEvent::ptr ev);
    // void error(LogEvent::ptr ev);
    // void fatal(LogEvent::ptr ev);

    // thread-safe 增加输出器
    void addAppender(LogAppender::ptr appender);
    // thread-safe 删除输出器
    void delAppender(LogAppender::ptr appender);

    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level level) { m_level = level; }

private:
    const std::string m_name;                    // 日志器名称
    LogLevel::Level m_level;                     // 日志有效级别
    std::string m_format_pattern;                // 日志输格式化器的默认pattern
    LogFormatter::ptr m_formatter;               // 日志默认格式化器，当加入 m_appender_list 的 appender 没有自己 formatter 时，使用该 Logger 的 formatter
    std::list<LogAppender::ptr> m_appender_list; // Appender列表
    Mutex m_mutex;
};

//输出到终端的Appender
class StdoutLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    explicit StdoutLogAppender(LogLevel::Level level = LogLevel::DEBUG);
    // thread-safe
    void log(LogLevel::Level level, LogEvent::ptr ev) override;
};

//输出到文件的Appender
class FileLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    explicit FileLogAppender(const std::string& filename, LogLevel::Level level = LogLevel::DEBUG);
    // ~FileLogAppender() override;
    void log(LogLevel::Level level, LogEvent::ptr ev) override;
    bool reopen();

private:
    std::string m_filename;
    std::ofstream m_file_stream;
};

/**
 * @brief 日志器的管理器
*/
class __LoggerManager
{
public:
    typedef std::shared_ptr<__LoggerManager> ptr;

    __LoggerManager();
    // 传入日志器名称来获取日志器,如果不存在,返回全局日志器
    Logger::ptr getLogger(const std::string& name);
    Logger::ptr getGlobal();

private:
    friend struct LogIniter;
    void init();
    void ensureGlobalLoggerExists(); // 确保存在全局日志器
    std::map<std::string, Logger::ptr> m_logger_map;
    Mutex m_mutex;
};

/**
 * @brief __LoggerManager 的单例类
*/
typedef SingletonPtr<__LoggerManager> LoggerManager;

struct LogIniter
{
    LogIniter()
    {
        auto log_config_list =
            zjl::Config::Lookup<std::vector<LogConfig>>("logs", {}, "日志器的配置项");
        // 注册日志器配置项变更事件处理器，当配置项变动时，更新日志器
        log_config_list->addListener(
            [](const std::vector<LogConfig>&, const std::vector<LogConfig>&) {
                std::cout << "日志器配置变动，更新日志器" << std::endl;
                LoggerManager::GetInstance()->init();
            });
    }
};
static LogIniter __log_init__;
}
#endif //SERVER_FRAMEWORK_LOG_H
