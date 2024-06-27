/**
 * @file log.h
 * @brief 日志模块封装
 */

#ifndef _LOG_H
#define _LOG_H

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream> 
#include <iostream>
#include <vector>
#include <stdarg.h>
#include <map>

/**
 * @brief 使用流方式将日志级别level的日志写入logger
 */
#define HPGS_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        HPGS_log::LogEventWrap(HPGS_log::LogEvent::ptr(new HPGS_log::LogEvent(logger, level, \
                                __FILE__, __LINE__, 0, HPGS_log::GetThreadId(), \
                                HPGS_log::GetFiberId(), time(0), HPGS_log::Thread::GetName()))).getSS()

namespace HPGS_log{

class LogEvent{
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent();

    const char* getFile() const { return m_file; }
    int32_t getLine() const { return m_line; }
    uint32_t getElapse() const { return m_elapse; }
    uint32_t getThreadId() const { return m_threadId; }
    uint32_t getFiberId() const { return m_fiberId; }
    uint64_t getTime() const { return m_time; }
    const std::string& getContent() const { return m_content; }
private:
    const char* m_file = nullptr;       //文件名
    int32_t m_line = 0;                 //行号
    uint32_t m_elapse = 0;              //程序启动到现在的毫秒数
    uint32_t m_threadId = 0;            //线程id
    uint32_t m_fiberId = 0;             //协程id
    uint64_t m_time = 0;                //时间戳
    std::string m_content;              
};

//日志级别
class LogLevel{
public:
    enum Level{
        UNKNOW = 0,
        DEBUG = 1,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    static const char* ToString(LogLevel::Level level);
};

/**
 * 日志格式器
 */
class LogFormatter{
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);
    
    std::string format(LogLevel::Level level, LogEvent::ptr event);

    class FormatItem{
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, LogLevel::Level level, LogEvent::ptr event) = 0;
    };
    void init();

private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
};


/**
 * 日志输出地
 */
class LogAppender{
public:
    typedef std::shared_ptr<LogAppender> ptr;
    virtual ~LogAppender(){}

    virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;
    void setFormatter(LogFormatter::ptr val) { m_formatter = val; }
    LogFormatter::ptr getFormatter() const { return m_formatter; }
protected:
    LogLevel::Level m_level;
    LogFormatter::ptr m_formatter;
};

/**
 * 日志输出器
*/    
class Logger{
public:
    typedef std::shared_ptr<Logger> ptr;
    
    Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warning(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level val) { m_level = val; }
private:
    std::string m_name;             //日志名称
    LogLevel::Level m_level;        //日志级别
    std::list<LogAppender::ptr> m_appenders;             //Appender集合
};

/**
 * 日志输出到stdout
 */
class StdOutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdOutLogAppender> ptr;
    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;
private:

};

/**
 * 日志输出到file
 */
class FileLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;

    void reopen();
private:
    std::string m_filename;
    std::ofstream m_filestream;
};



}

#endif