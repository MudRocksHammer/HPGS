/**
 * @file log.h
 * @brief 日志模块封装
 */

#ifndef __LOG_H__
#define __LOG_H__

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
#include "singleton.h"

/**
 * @brief 使用流方式将日志级别level的日志写入logger
 * @details 构造一个LogEventWrap对象，包裹包含日志器和日志事件，再对象析构时调用日志器写日志事件
 */
/* #define HPGS_LOG_LEVEL(logger, level) \
//     if(logger->getLevel() <= level) \
//         HPGS::LogEventWrap(HPGS::LogEvent::ptr(new HPGS::LogEvent(logger, level, \
//                                 __FILE__, __LINE__, 0, HPGS::GetThreadId(), \
//                                 HPGS::GetfibreId(), time(0), HPGS::Thread::GetName()))).getSS()
*/
#define HPGS_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        HPGS::LogEventWrap(HPGS::LogEvent::ptr(new HPGS::LogEvent(logger, level, \
                                __FILE__, __LINE__, 0, 0, \
                                1, time(0), "thread1"))).getSS()

/**
 * @brief 使用流方式将日志级别debug的日志写到logger
 */
#define HPGS_LOG_DEBUG(logger) HPGS_LOG_LEVEL(logger, HPGS::LogLevel::DEBUG)

/**
 * @brief 使用流方式将日志级别info的日志写到logger
 */
#define HPGS_LOG_INFO(logger) HPGS_LOG_LEVEL(logger, HPGS::LogLevel::INFO)

/**
 * @brief 使用流方式将日志级别warning的日志写到logger
 */
#define HPGS_LOG_WARNING(logger) HPGS_LOG_LEVEL(logger, HPGS::LogLevel::WARNING)

/**
 * @brief 使用流方式将日志级别error的日志写到logger
 */
#define HPGS_LOG_ERROR(logger) HPGS_LOG_LEVEL(logger, HPGS::LogLevel::ERROR)

/**
 * @brief 使用流方式将日志级别fatal的日志写到logger
 */
#define HPGS_LOG_FATAL(logger) HPGS_LOG_LEVEL(logger, HPGS::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 */
#define HPGS_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        HPGS::LogEventWrap(HPGS::LogEvent::ptr(new HPGS::LogEvent(logger, level, \
                            __FILE__, __LINE__, 0, HPGS::GetThreadId(), \
                        HPGS::GetFibreId(), time(0), HPGS::THREAD::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别debug的日志写入到logger
 */
#define HPGS_LOG_FMT_DEBUG(logger, fmt, ...) HPGS_LOG_FMT_LEVEL(logger, HPGS::LogLevel::DEBUG, fmt, __VA_ARGS)

/**
 * @brief 使用格式化方式将日志级别info的日志写入到logger
 */
#define HPGS_LOG_FMT_INFO(logger, fmt, ...) HPGS_LOG_FMT_LEVEL(logger, HPGS::LogLevel::INFO, fmt, __VA_ARGS)

/**
 * @brief 使用格式化方式将日志级别warning的日志写入到logger
 */
#define HPGS_LOG_FMT_WARNING(logger, fmt, ...) HPGS_LOG_FMT_LEVEL(logger, HPGS::LogLevel::WARNING, fmt, __VA_ARGS)

/**
 * @brief 使用格式化方式将日志级别error的日志写入到logger
 */
#define HPGS_LOG_FMT_ERROR(logger, fmt, ...) HPGS_LOG_FMT_LEVEL(logger, HPGS::LogLevel::ERROR, fmt, __VA_ARGS)

/**
 * @brief 使用格式化方式将日志级别fatal的日志写入到logger
 */
#define HPGS_LOG_FMT_FATAL(logger, fmt, ...) HPGS_LOG_FMT_LEVEL(logger, HPGS::LogLevel::FATAL, fmt, __VA_ARGS)

/**
 * @brief 获取主日志器
 */
#define HPGS_LOG_ROOT() HPGS::LoggerMgr::GetInstance()->getRoot()

/**
 * @brief 获取名为name的日志器
 */
#define HPGS_LOG_NAME(name) HPGS::LoggerMgr::GetInstance()->getLogger(name)

/**
 * @brief 通过logger和event实例写日志信息
 */
#define HPGS_WRITE_LOG(logger, event, message) \
        if(logger->getLevel() <= event->getLevel()) {\
            event->setContent(message); \
            logger->log(event->getLevel(), event);  \
        }

namespace HPGS{

class Logger;
class LoggerManager;

/**
 * @brief 日志级别
 */
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

    /**
     * @brief 将日志级别转换成文本输出
     * @param[in] str 日志级别
     */
    static const char* ToString(LogLevel::Level level);

    /**
     * @brief 将日志文本转换为日志级别
     * @param[in] str 日志级别文本
     */
    static LogLevel::Level FromString(const std::string& str);
};

/**
 * @brief 日志事件
 */
class LogEvent{
public:
    typedef std::shared_ptr<LogEvent> ptr;

    /**
     * @brief 日志事件的构造函数
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] file  文件名
     * @param[in] line  文件行号
     * @param[in] elapse    程序启动依赖的耗时
     * @param[in] thread_id 线程id
     * @param[in] fibre_id  协程id
     * @param[in] time      日志时间(s)
     * @param[in] thread_bane   线程名称
     */
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
             const char* file, int32_t line, uint32_t elapse,
             uint32_t thread_id, uint32_t fibre_id, uint64_t time,
             const std::string& thread_name);

    const char* getFile() const { return m_file; }
    int32_t getLine() const { return m_line; }
    uint32_t getElapse() const { return m_elapse; }
    uint32_t getThreadId() const { return m_threadId; }
    uint32_t getFibreId() const { return m_fibreId; }
    uint64_t getTime() const { return m_time; }
    const std::string& getThreadNmae() const { return m_threadName; }
    const std::string& getContent() const { return m_content; }
    std::shared_ptr<Logger> getLogger() const { return m_logger; }
    LogLevel::Level getLevel() const { return m_level; }
    std::stringstream& getSS() { return m_ss; }
    void setContent(const std::string& str) { m_content = str; }

    /**
     * @brief 格式化写入日志内容
     */
    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);
      
private:
    const char* m_file = nullptr;       //文件名
    int32_t m_line = 0;                 //行号
    uint32_t m_elapse = 0;              //程序启动到现在的毫秒数
    uint32_t m_threadId = 0;            //线程id
    uint32_t m_fibreId = 0;             //协程id
    uint64_t m_time = 0;                //时间戳
    std::string m_threadName;           //线程名
    std::string m_content;              //日志内容
    std::shared_ptr<Logger> m_logger;               //日志器
    LogLevel::Level m_level;            //日志级别
    std::stringstream m_ss;             //日志字符串流
};

/**
 * @brief 日志事件包装器
 */
class LogEventWrap{
public:
    /**
     * @brief 构造函数
     * @param[in] e 日志事件
     */
    LogEventWrap(LogEvent::ptr e);

    /**
     * @brief 析构函数
     */
    ~LogEventWrap();

    /**
     * @brief 获取日志事件
     */
    LogEvent::ptr getEvent() const { return m_event; }

    /**
     * @brief 获取日志内容流
     */
    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};

/** 
 * @brief 日志格式器
 */
class LogFormatter{
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details 
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     */
    LogFormatter(const std::string& pattern);
    
    /**
     * @brief 返回格式化日志文本
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
   
    /**
     * @brief 日志内容项格式化
     */
    class FormatItem{
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        FormatItem(const std::string& fmt = "") {}
        virtual ~FormatItem() {}

        /**
         * @brief 格式化到日志流
         * @param[in, out] os 日志输出流
         * @param[in] logger
         * @param[in] level
         * @param[in] event
         */
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    /**
     * @brief 初始化，解析日志模板
     */
    void init();

    /**
     * @brief 是否有错误
     */
    bool isError() const { return m_error; }

    /**
     * @brief 返回日志模板
     */
    const std::string getPattern() const { return m_pattern; }

private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;       //日志格式解析后格式
    bool m_error;
};


/**
 * 日志输出目标
 */
class LogAppender{
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    //typedef Spinlock MutexType;

    virtual ~LogAppender(){}

    /**
     * @brief 写入日志
     */
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    /**
     * @brief 将日志输出目标的配置转成YAML String
     */
    //virtual std::string toYamlString() = 0;

    /**
     * @brief 更改日志格式器
     */
    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getFormatter();

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel::Level level) { m_level = level; }
    LogLevel::Level getLevel() const { return m_level; }

protected:
    LogLevel::Level m_level;
    bool m_hasFormatter = false;            //是否拥有自己的日志格式器
    //MutexType m_mutex;                    //mutex
    LogFormatter::ptr m_formatter;          //日志格式器
};

/**
 * 日志输出器
*/    
class Logger : public std::enable_shared_from_this<Logger>{
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    //typedef Spinlock MutexType;
    
    /**
     * @brief Logger类构造函数
     * @param[in] name 日志器名称
     */
    Logger(const std::string& name = "root");

    /**
     * @brief 写日志
     * @param[in] level
     * @param[in] event
     */
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warning(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppender();

    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level val) { m_level = val; }

    /**
     * @brief 返回日志名称
     */
    const std::string& getName() const { return m_name; }

    /**
     * @brief 设置日志格式器
     */
    void setFormatter(LogFormatter::ptr val);
    /**
     * @brief 设置日志格式模板
     */
    void setFormatter(const std::string& val);

    LogFormatter::ptr getFormatter();
    //std::string toYamlString();

private:
    std::string m_name;             //日志名称
    LogLevel::Level m_level;        //日志级别
    //MutexType m_mutex;
    std::list<LogAppender::ptr> m_appenders;             //Appender集合
    LogFormatter::ptr m_formatter;  //日志格式器
    Logger::ptr m_root;             //主日志器
};

/**
 * 日志输出到stdout
 */
class StdOutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdOutLogAppender> ptr;
    void log(Logger::ptr logger ,LogLevel::Level level, LogEvent::ptr event) override;
    //std::string toYamlString() override;
private:

};

/**
 * 日志输出到file
 */
class FileLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    //std::string toYamlString() override;

    /**
     * @brief 重新打开日志文件
     */
    bool reopen();

private:
    std::string m_filename;         //文件路径
    std::ofstream m_filestream;     //文件流
    uint64_t m_lastTime = 0;        //上次重新打开时间
};

/**
 * @brief 日志器管理类
 */
class LoggerManager{
public:
    //typedef Spinlock MutexType;
    LoggerManager();

    /**
     * @brief 获取日志器
     * @param[in] name 日志器名称
     */
    Logger::ptr getLogger(const std::string& name);

    /**
     * @brief 初始化
     */
    void init();
    
    /**
     * @brief 返回主日志器
     */
    Logger::ptr getRoot() const { return m_root; }

    /**
     * @brief 将所有日志器配置转成YAML String
     */
    //std::string toYamlString();

private:
    //MutexType m_mutex;
    std::map<std::string, Logger::ptr> m_loggers;       //日志器容器
    Logger::ptr m_root;                                 //主日志器
};

/// 日志器管理类单例模式
typedef HPGS::Singleton<LoggerManager> LoggerMgr;

}

#endif