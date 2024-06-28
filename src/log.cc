#include "log.h"
#include <time.h>
#include <iostream>
#include <functional>
#include <string.h>
#include <map>

namespace HPGS{

const char* LogLevel::ToString(LogLevel::Level level){
    switch(level){
#define XX(name) \
    case LogLevel::name: \
        return #name;   \
        break;
    
    XX(DEBUG);
    XX(INFO);
    XX(WARNING);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKNOW";
        break;
    }
    return "UNKNOW";
}

LogLevel::Level LogLevel::FromString(const std::string& str){
#define XX(level, v) \
    if(str == #v){  \
        return LogLevel::level; \
    }
    XX(DEBUG, debug);
    XX(INFO, info);
    XX(WARNING, warning);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARNING, WARNING);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOW;
#undef XX
}

LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e) { }

void LogEvent::format(const char* fmt, ...){
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al){
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1){
        m_ss << std::string(buf, len);
        free(buf);
    }
}

std::stringstream& LogEventWrap::getSS(){
    return m_event->getSS();
}

void LogAppender::setFormatter(LogFormatter::ptr val){
    //MutexType::Lock lock(m_mutex);
    m_formatter = val;
    if(m_formatter){
        m_hasFormatter = true;
    }
    else{
        m_hasFormatter = false;
    }
}

LogFormatter::ptr LogAppender::getFormatter(){
    //MutexType::Lock lock(m_mutex);
    return m_formatter;
}

class MessageFormatItem : public LogFormatter::FormatItem{
public:
    MessageFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }

};

class LevelFormatItem : public LogFormatter::FormatItem{
public:
    LevelFormatItem(const std::string& std = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }

};

class ElapseFormatItem : public LogFormatter::FormatItem{
public:
    ElapseFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class NameFormatItem : public LogFormatter::FormatItem{
public:
    NameFormatItem(const std::string& std = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem{
public:
    ThreadIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }

};

class FibreIdFormatItem : public LogFormatter::FormatItem{
public:
    FibreIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFibreId();
    }

};

class ThreadNameFormatItem : public LogFormatter::FormatItem{
public:
    ThreadNameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadNmae();
    }

};

class DateTimeFormatItem : public LogFormatter::FormatItem{
public:
    DateTimeFormatItem(const std::string& format = "%Y:%m:%d %H:%M:%S") : m_format(format){
        if(m_format.empty()){
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem{
public:
    FileNameFormatItem(const std::string& std = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }

};

class LineFormatItem : public LogFormatter::FormatItem{
public:
    LineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }

};    

class NewLineFormatItem : public LogFormatter::FormatItem{
public:
    NewLineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }

};    

class StringFormatItem : public LogFormatter::FormatItem{
public:
    StringFormatItem(const std::string& str) : m_string(str) {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }

private:
    std::string m_string;
};    

class TabFormatItem : public LogFormatter::FormatItem{
public:
    TabFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }

};    

LogEvent::LogEvent(Logger::ptr logger, LogLevel::Level level,
                    const char* file, int32_t line, uint32_t elapse,
                    uint32_t thread_id, uint32_t fibre_id, uint64_t time,
                    const std::string& thread_name)
            : m_file(file),
            m_line(line),
            m_elapse(elapse),
            m_threadId(thread_id),
            m_fibreId(fibre_id),
            m_time(time),
            m_threadName(thread_name),
            m_logger(logger),
            m_level(level)  {  }

Logger::Logger(const std::string& name) 
        : m_name(name),
        m_level(LogLevel::DEBUG) {
            m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H-%M-%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::setFormatter(LogFormatter::ptr val){
    //MutexType::Lock lock(m_mutex);
    m_formatter = val;

    for(auto& i : m_appenders){
        //MutexType::Lock ll(i->m_mutex);
        if(!i->m_hasFormatter){
            i->m_formatter = m_formatter;
        }
    }
}

void Logger::setFormatter(const std::string& val){
    std::cout << "---" << val << std::endl;
    //LogFormatter::ptr new_val(new LogFormatter(val));
    LogFormatter::ptr new_val = std::make_shared<LogFormatter>(val);
    if(new_val->isError()){
        std::cout << "Logger setFormatter name=" << m_name
                  << "value = " << val << " invalid formatter"
                  << std::endl;
        return;
    }

    setFormatter(new_val);
}
/*
std::string Logger::toYamlString(){
    //MutexType::Lock lock(m_mutex);
    YAML::Node node;
}
*/

LogFormatter::ptr Logger::getFormatter() {
    //MutexType::Lock lock(m_mutex);
    return m_formatter;
}

void Logger::addAppender(LogAppender::ptr appender){
    //MutexType::Lock lock(m_mutex);
    if(!appender->getFormatter()){
        //MutexType::Lock ll(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender){
    //MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it != m_appenders.end(); it++){
        if(*it == appender){
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event){
    if(level >= m_level){
        auto self = shared_from_this();
        //MutexType::Lock lock(m_mutex);
        if(!m_appenders.empty()){
            for(auto& i : m_appenders){
                i->log(self, level, event);
            }
        }
        else if(m_root){
            m_root->log(level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event){
    log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event){
    log(LogLevel::INFO, event);
}

void Logger::warning(LogEvent::ptr event){
    log(LogLevel::WARNING, event);
}

void Logger::error(LogEvent::ptr event){
    log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event){
    log(LogLevel::FATAL, event);
}

FileLogAppender::FileLogAppender(const std::string& filename) : m_filename(filename){
    reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event){
    if(level >= m_level){
        uint64_t now = event->getTime();
        if(now >= (m_lastTime + 3)){
            reopen();
            m_lastTime = now;
        }
        //MutexType::Lock lock(m_mutex);
        //m_filestream << m_formatter.format(logger, level, event);
        if(!m_formatter->format(m_filestream, logger, level, event)){
            std::cout << "error" << std::endl;
        }
    }
}

// std::string FileLogAppender::toYamlString(){

// }

bool FileLogAppender::reopen(){
    //MutexType::Lock lock(m_mutex);
    if(m_filestream){
        m_filestream.close();
    }
    m_filestream.open(m_filename);

    return !!m_filestream;
}

void StdOutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event){
    if(level >= m_level){
        std::cout << m_formatter->format(logger, level, event);
    }
}

LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern){

}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event){
    std::stringstream ss;
    for(auto& i: m_items){
        i->format(ss, level, event);
    }
    return ss.str();
}

//%xxx %xxx{xxx} %%
void LogFormatter::init(){
    //std format type
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;
    for(size_t i = 0; i < m_pattern.size(); i++){
        if(m_pattern[i] != '%'){
            nstr.append(1, m_pattern[i]);
            continue;
        }

        if((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%'){
            nstr.append(1, '%');
            continue;
        }

        size_t n = i + 1, fmt_begin = 0;
        int fmt_status = 0;

        std::string str, fmt;
        while(n < m_pattern.size()){
            if(isspace(m_pattern[n])){
                break;
            }
            if(fmt_status == 0){
                if(m_pattern[n] == '{'){
                    str = m_pattern.substr(i + 1, n - i);
                    fmt_status = 1;     //  解析格式
                    fmt_begin = n;
                    n++;
                    continue;
                }
            }
            if(fmt_status == 1){
                if(m_pattern[n] == '}'){
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 2;
                    break;
                }
            }
        }

        if(fmt_status == 0){
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, "", 0));
            }
            str = m_pattern.substr(i + 1, n - i - 1);
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n;
        }
        else if(fmt_status == 1){
            std::cout << "Pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            vec.push_back(std::make_tuple("<Pattern Error>", fmt, 0));
        }
        else if(fmt_status == 2){
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, "", 0));
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n;
        }

    }

    if(!nstr.empty()){
        vec.push_back(std::make_tuple(nstr, "", 0));
    }

    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> 
            s_format_items = {
#define XX(str, C)  \
        {str, [](const std::string& fmt) {return FormatItem::ptr(new C(fmt)); }}    \
            XX(m, MessageFormatItem);           //m:消息
            XX(p, LevelFormatItem);             //p:日志级别
            XX(r, ElapseFormatItem);            //r:累计毫秒数
            XX(c, NameFormatItem);              //c：日志名称
            XX(t, ThreadIdFormatItem);          //t：线程id
            XX(n, NewLineFormatItem);           //n：换行
            XX(d, DateTimeFormatItem);          //d: 日期时间
            XX(f, FilenameFormatItem);          //f：文件名
            XX(l, LineFormatItem);              //l：行号
            XX(T, TabFormatItem);               //T：tab
            XX(F, FibreFormatItem);             //F：协程ID
            XX(N, ThreadNameFormatItem);        //N：线程名称
#undef XX
    };

    for(autp& i : vec){
        if(std::get<2>(i) == 0){
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        }
        else{
            auto it = s_format_items.find(std::get<0>(i));
            if(it == s_format_items.end()){
                m_items.push_back(FOrmatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
            }
            else{
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }
        std::cout << std::get<0>(i) << "-" << std::get<1>(i) << "-" <<std::get<2>(i) << std::endl;
    }
    /**
     * %m -- 消息体
     * %p -- level
     * %r -- 启动后时间
     * %c -- 日志名称
     * %t -- 线程id
     * %n -- 回车换行
     * %d -- 时间
     * %f -- 文件名
     * %l -- 行号
     */
}

         

}