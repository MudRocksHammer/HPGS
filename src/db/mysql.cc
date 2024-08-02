#include "HPmysql.h"
#include "log.h"
#include "config.h"

namespace HPGS{

static HPGS::Logger::ptr g_logger = HPGS_LOG_NAME("system");
static HPGS::ConfigVar<std::map<std::string, std::map<std::string, std::string> > >::ptr g_mysql_dbs
        = HPGS::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string> >(), "mysql dbs");

bool mysql_time_to_time_t(const MYSQL_TIME& mt, time_t& ts){
    struct tm tm;
    ts = 0;
    localtime_r(&ts, &tm);
    tm.tm_year = mt.year - 1900;
    tm.tm_mon = mt.month - 1;
    tm.tm_mday = mt.day;
    tm.tm_hour = mt.hour;
    tm.tm_min = mt.minute;
    tm.tm_sec = mt.second;
    ts = mktime(&tm);
    if(ts < 0){
        ts = 0;
    }
    return true;
}

bool time_t_to_mysql_time(const time_t& ts, MYSQL_TIME& mt){
    struct tm tm;
    localtime_r(&ts, &tm);
    mt.year = tm.tm_year + 1900;
    mt.month = tm.tm_mon + 1;
    mt.day = tm.tm_mday;
    mt.hour = tm.tm_hour;
    mt.minute = tm.tm_min;
    mt.second = tm.tm_sec;
    return true;
}

namespace{

struct MySQLThreadIniter{
        MySQLThreadIniter(){
            mysql_thread_init();
        }

        ~MySQLThreadIniter(){
            mysql_thread_end();
        }
};

}

static MYSQL* mysql_init(std::map<std::string, std::string>& params, const int& timeout){
    static thread_local MySQLThreadIniter s_thread_initer;

    MYSQL* mysql = ::mysql_init(nullptr);
    if(mysql == nullptr){
        HPGS_LOG_ERROR(g_logger) << "mysql_init error";
        return nullptr;
    }

    if(timeout > 0){
        mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    }
    bool close = false;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &close);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    int port = HPGS::GetParamValue(params, "port", 0);
    std::string host = HPGS::GetParamValue<std::string>(params, "host");
    std::string user = HPGS::GetParamValue<std::string>(params, "user");
    std::string passwd = HPGS::GetParamValue<std::string>(params, "passwd");
    std::string dbname = HPGS::GetParamValue<std::string>(params, "dbname");

    if(mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, NULL, 0) == nullptr){
        HPGS_LOG_ERROR(g_logger) << "mysql_real_connect(" << host
                << ", " << port << ", " << dbname << ") error: " << mysql_error(mysql);
        mysql_close(mysql);
        return nullptr;
    }
    return mysql;
}

MySQL::MySQL(const std::map<std::string, std::string>& args)
: m_params(args), m_lastUsedTime(0), m_hasError(false), m_poolSize(10){

}

bool MySQL::connect(){
    if(m_mysql && !m_hasError){
        return true;
    }

    MYSQL* m = mysql_init(m_params, 0);
    if(!m){
        m_hasError = true;
        return false;
    }

    m_hasError = false;
    m_poolSize = HPGS::GetParamValue(m_params, "pool", 5);
    m_mysql.reset(m, mysql_close);
    return true;
}

HPGS::IStmt::ptr MySQL::prepare(const std::string& sql){
    return MySQLStmt::Create(shared_from_this(), sql);
}

ITransaction::ptr MySQL::openTransaction(bool auto_commit){
    return MySQLTransaction::Create(shared_from_this(), auto_commit);
}

int64_t MySQL::getLastInsertId(){
    return mysql_insert_id(m_mysql.get());
}

bool MySQL::isNeedCheck(){
    if((time(0) - m_lastUsedTime) < 5 && !m_hasError){
        return false;
    }
    return true;
}

bool MySQL::ping(){
    if(!m_mysql){
        return false;
    }

    if(mysql_ping(m_mysql.get())){
        m_hasError = true;
        return false;
    }
    m_hasError = false;
    return true;
}

int MySQL::execute(const char* format, ...){
    va_list ap;
    va_start(ap, format);
    int rt = execute(format, ap);
    va_end(ap);
    return rt;
}

int MySQL::execute(const char* format, va_list ap){
    m_cmd = HPGS::StringUtil::Formatv(format, ap);
    int r = ::mysql_query(m_mysql.get(), m_cmd.c_str());
    if(r){
        HPGS_LOG_ERROR(g_logger) << "cmd = " << cmd()
                << ", error: " << getErrStr();
        m_hasError = true;
    }
    else{
        m_hasError = false;
    }
    return r;
}



}