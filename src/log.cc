//
// Created by zjlian on 2020/1/27.
//

#include "log.h"
#include <algorithm>
#include <ctime>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

namespace zjl
{

class PlainFormatItem : public LogFormatter::FormatItem
{
public:
    explicit PlainFormatItem(const std::string& str) : m_str(str) {}
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << m_str;
    }

private:
    std::string m_str;
};

class LevelFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << LogLevel::levelToString(ev->getLevel());
    }
};

class FilenameFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << ev->getFilename();
    }
};

class LineFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << ev->getLine();
    }
};

class ThreadIDFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << ev->getThreadId();
    }
};

class FiberIDFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << ev->getFiberId();
    }
};

class TimeFormatItem : public LogFormatter::FormatItem
{
public:
    explicit TimeFormatItem(const std::string& str = "%Y-%m-%d %H:%M:%S")
        : m_time_pattern(str)
    {
        if (m_time_pattern.empty())
        {
            m_time_pattern = "%Y-%m-%d %H:%M:%S";
        }
    }
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        struct tm time_struct
        {
        };
        time_t time_l = ev->getTime();
        localtime_r(&time_l, &time_struct);
        char buffer[64]{0};
        strftime(buffer, sizeof(buffer),
                 m_time_pattern.c_str(), &time_struct);
        out << buffer;
    }

private:
    std::string m_time_pattern;
};

class ContentFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << ev->getContent();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << std::endl;
    }
};

class PercentSignFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << '%';
    }
};

class TabFormatItem : public LogFormatter::FormatItem
{
public:
    void format(std::ostream& out, LogEvent::ptr ev) override
    {
        out << '\t';
    }
};
/**
 * %p ??????????????????
 * %f ???????????????
 * %l ????????????
 * %d ??????????????????
 * %t ???????????????
 * %F ???????????????
 * %m ??????????????????
 * %n ????????????
 * %% ???????????????
 * %T ???????????????
 * */
thread_local static std::map<char, LogFormatter::FormatItem::ptr> format_item_map{
#define FN(CH, ITEM_NAME)                 \
    {                                     \
        CH, std::make_shared<ITEM_NAME>() \
    }
    FN('p', LevelFormatItem),
    FN('f', FilenameFormatItem),
    FN('l', LineFormatItem),
    FN('d', TimeFormatItem),
    FN('t', ThreadIDFormatItem),
    FN('F', FiberIDFormatItem),
    FN('m', ContentFormatItem),
    FN('n', NewLineFormatItem),
    FN('%', PercentSignFormatItem),
    FN('T', TabFormatItem),
#undef FN
};

std::string LogLevel::levelToString(LogLevel::Level level)
{
    std::string result;
    switch (level)
    {
        case DEBUG:
            result = "DEBUG";
            break;
        case INFO:
            result = "INFO";
            break;
        case WARN:
            result = "WARN";
            break;
        case ERROR:
            result = "ERROR";
            break;
        case FATAL:
            result = "FATAL";
            break;
        case UNKNOWN:
            result = "UNKNOWN";
            break;
    }
    return result;
}

Logger::Logger()
    : m_name("default"),
      m_level(LogLevel::DEBUG),
      m_format_pattern("[%d] [%p] [T:%t F:%F]%T%m%n")
{
    m_formatter.reset(new LogFormatter(m_format_pattern));
}

Logger::Logger(const std::string& name, LogLevel::Level level, const std::string& pattern)
    : m_name(name), m_level(level), m_format_pattern(pattern)
{
    m_formatter.reset(new LogFormatter(pattern));
}

void Logger::addAppender(LogAppender::ptr appender)
{
    ScopedLock lock(&m_mutex);
    if (!appender->getFormatter())
    {
        appender->setFormatter(m_formatter);
    }
    m_appender_list.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender)
{
    ScopedLock lock(&m_mutex);
    // TODO ????????????????????????
    auto itor = std::find(m_appender_list.begin(), m_appender_list.end(), appender);
    if (itor != m_appender_list.end())
    {
        m_appender_list.erase(itor);
    }
}

void Logger::log(LogEvent::ptr ev)
{
    // ???????????????????????????????????????????????????????????????????????????
    if (ev->getLevel() < m_level)
    {
        return;
    }
    // ??????????????????????????????
    ScopedLock lock(&m_mutex);
    for (auto& item : m_appender_list)
    {
        item->log(ev->getLevel(), ev);
    }
}

// void Logger::debug(LogEvent::ptr ev)
// {
//     ev->setLevel(LogLevel::DEBUG);
//     log(ev);
// }

// void Logger::info(LogEvent::ptr ev)
// {
//     ev->setLevel(LogLevel::INFO);
//     log(ev);
// }

// void Logger::warn(LogEvent::ptr ev)
// {
//     ev->setLevel(LogLevel::WARN);
//     log(ev);
// }

// void Logger::error(LogEvent::ptr ev)
// {
//     ev->setLevel(LogLevel::ERROR);
//     log(ev);
// }

// void Logger::fatal(LogEvent::ptr ev)
// {
//     ev->setLevel(LogLevel::FATAL);
//     log(ev);
// }

LogFormatter::ptr LogAppender::getFormatter()
{
    ScopedLock lock(&m_mutex);
    return m_formatter;
}

void LogAppender::setFormatter(LogFormatter::ptr formatter)
{
    ScopedLock lock(&m_mutex);
    m_formatter = std::move(formatter);
}

StdoutLogAppender::StdoutLogAppender(LogLevel::Level level)
    : LogAppender(level) {}

void StdoutLogAppender::log(LogLevel::Level level, LogEvent::ptr ev)
{
    if (level < m_level)
    {
        return;
    }
    ScopedLock lock(&m_mutex);
    std::cout << m_formatter->format(ev);
    std::cout.flush();
}

LogAppender::LogAppender(LogLevel::Level level)
    : m_level(level) {}

FileLogAppender::FileLogAppender(const std::string& filename, LogLevel::Level level)
    : LogAppender(level), m_filename(filename)
{
    reopen();
}

// FileLogAppender::~FileLogAppender() override {
//     if (!m_file_stream) {
//         m_file_stream.close();
//     }
// }

bool FileLogAppender::reopen()
{
    ScopedLock lock(&m_mutex);
    // ?????? fstream::operator!() ?????????????????????????????????
    if (!m_file_stream)
    {
        m_file_stream.close();
    }
    m_file_stream.open(m_filename, std::ios_base::out | std::ios_base::app);
    return !!m_file_stream;
}

void FileLogAppender::log(LogLevel::Level level, LogEvent::ptr ev)
{
    if (level < m_level)
    {
        return;
    }
    ScopedLock lock(&m_mutex);
    m_file_stream << m_formatter->format(ev);
    m_file_stream.flush();
}

LogFormatter::LogFormatter(const std::string& pattern)
    : m_format_pattern(pattern)
{
    init();
}

std::string LogFormatter::format(LogEvent::ptr ev)
{
    std::stringstream ss;
    for (auto& item : m_format_item_list)
    {
        item->format(ss, ev);
    }
    return ss.str();
}

// %d {%d:%d} aaaaaa %d %n
void LogFormatter::init()
{
    enum PARSE_STATUS
    {
        SCAN_STATUS,   // ??????????????????
        CREATE_STATUS, // ????????? %??????????????????
    };
    PARSE_STATUS STATUS = SCAN_STATUS;
    size_t str_begin = 0, str_end = 0;
    //    std::vector<char> item_list;
    for (size_t i = 0; i < m_format_pattern.length(); i++)
    {
        switch (STATUS)
        {
            case SCAN_STATUS: // ???????????????????????????????????????????????????????????????????????????????????????????????? m_format_item_list ???
                // ?????????????????????????????????????????????
                str_begin = i;
                for (str_end = i; str_end < m_format_pattern.length(); str_end++)
                {
                    // ????????? % ????????????????????????????????? STATUS ?????????????????????????????????????????????????????????????????????????????????
                    if (m_format_pattern[str_end] == '%')
                    {
                        STATUS = CREATE_STATUS;
                        break;
                    }
                }
                i = str_end;
                m_format_item_list.push_back(
                    std::make_shared<PlainFormatItem>(
                        m_format_pattern.substr(str_begin, str_end - str_begin)));
                break;

            case CREATE_STATUS: // ???????????????
                assert(!format_item_map.empty() && "format_item_map ???????????????????????????");
                auto itor = format_item_map.find(m_format_pattern[i]);
                if (itor == format_item_map.end())
                {
                    m_format_item_list.push_back(std::make_shared<PlainFormatItem>("<error format>"));
                }
                else
                {
                    m_format_item_list.push_back(itor->second);
                }
                STATUS = SCAN_STATUS;
                break;
        }
    }
}

__LoggerManager::__LoggerManager()
{
    init();
}

void __LoggerManager::ensureGlobalLoggerExists()
{
    // std::map ???????????? operator[] ??????????????????????????????????????????????????????????????????????????????????????????
    auto iter = m_logger_map.find("global");
    if (iter == m_logger_map.end())
    { // ????????? map ???????????????????????????
        auto global_logger = std::make_shared<Logger>();
        global_logger->addAppender(std::make_shared<StdoutLogAppender>());
        m_logger_map.insert(std::make_pair("global", std::move(global_logger)));
    }
    else if (!iter->second)
    { // ????????????????????????????????????
        iter->second = std::make_shared<Logger>();
        iter->second->addAppender(std::make_shared<StdoutLogAppender>());
    }
}

void __LoggerManager::init()
{
    ScopedLock lock(&m_mutex);
    auto config = Config::Lookup<std::vector<LogConfig>>("logs");
    const auto& config_log_list = config->getValue();
    for (const auto& config_log : config_log_list)
    {
        // ??????????????????????????? logger
        m_logger_map.erase(config_log.name);
        auto logger = std::make_shared<Logger>(
            config_log.name, config_log.level, config_log.formatter);
        for (const auto& config_app : config_log.appender)
        {
            LogAppender::ptr appender;
            switch (config_app.type)
            {
                // ???????????????????????????
                case LogAppenderConfig::Stdout:
                    appender = std::make_shared<StdoutLogAppender>(config_app.level);
                    break;
                // ???????????????????????????
                case LogAppenderConfig::File:
                    appender = std::make_shared<FileLogAppender>(
                        config_app.file, config_app.level);
                    break;
                default:
                    std::cerr << "LoggerManager::init exception ????????? appender ????????????appender.type=" << config_app.type << std::endl;
                    break;
            }
            // ??????????????? appender ??????????????????????????????????????? formatter
            // ?????????????????? logger ????????????????????? logger ???????????? formatter
            if (!config_app.formatter.empty())
            {
                appender->setFormatter(
                    std::make_shared<LogFormatter>(config_app.formatter));
            }
            logger->addAppender(std::move(appender));
        }
        std::cout << "????????????????????? " << config_log.name << std::endl;
        m_logger_map.insert(std::make_pair(config_log.name, std::move(logger)));
    }
    // ????????????????????????????????????
    ensureGlobalLoggerExists();
}

Logger::ptr __LoggerManager::getLogger(const std::string& name)
{
    ScopedLock lock(&m_mutex);
    auto iter = m_logger_map.find(name);
    if (iter == m_logger_map.end())
    {
        // ????????????????????????????????????????????????
        return m_logger_map.find("global")->second;
    }
    return iter->second;
}

Logger::ptr __LoggerManager::getGlobal()
{
    return getLogger("global");
}
}
