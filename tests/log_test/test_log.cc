#include <iostream>
#include "log.h"
#include <string>
#include <gtest/gtest.h>
#include <fstream>

bool fileExists(const std::string& filename){
    std::ifstream file(filename);
    return file.good();
}

std::string readFile(const std::string& filename){
    std::ifstream file(filename);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

TEST(LOG_TEST, INFO_1){
    //system("../bin/test");
    HPGS::Logger::ptr logger = std::make_shared<HPGS::Logger>();
    logger->addAppender(HPGS::LogAppender::ptr(new HPGS::StdOutLogAppender));

    HPGS::FileLogAppender::ptr file_appender = std::make_shared<HPGS::FileLogAppender>("./log.txt");
    HPGS::LogFormatter::ptr fmt = std::make_shared<HPGS::LogFormatter>("%d%T%p%T%m%n");
    file_appender->setFormatter(fmt);
    file_appender->setLevel(HPGS::LogLevel::INFO);
    logger->addAppender(file_appender);

    HPGS::LogEvent::ptr ev = std::make_shared<HPGS::LogEvent>(
        logger, HPGS::LogLevel::FATAL, __FILE__, __LINE__, 0, 1, 2, time(0), std::string("th1"));
    //HPGS_WRITE_LOG(logger, ev, "This is google test");
    HPGS_LOG_INFO(logger) << "test stream";
    //HPGS_WRITE_LOG(logger, ev, )
    
    //Test 
    ASSERT_TRUE(fileExists("log.txt"));
    
    std::string content = readFile("log.txt");
    ASSERT_EQ(content, "abc");
}

int main(int argc, char* argv[]){
    //HPGS::Logger::ptr logger(new HPGS::Logger);
    // HPGS::Logger::ptr logger = std::make_shared<HPGS::Logger>();
    // logger->addAppender(HPGS::LogAppender::ptr(new HPGS::StdOutLogAppender));

    // HPGS::FileLogAppender::ptr file_appender(new HPGS::FileLogAppender("./log.txt"));
    // HPGS::LogFormatter::ptr fmt(new HPGS::LogFormatter("%d%T%p%T%m%n"));
    // file_appender->setFormatter(fmt);
    // file_appender->setLevel(HPGS::LogLevel::ERROR);
    // logger->addAppender(file_appender);

    // //HPGS::LogEvent::ptr event(new HPGS::LogEvent(__FILE__, __LINE__, 0, HPGS::GetThreadId(), HPGS::GetFibreId(), time(0)));
    // HPGS::LogEvent::ptr event(new HPGS::LogEvent(logger, HPGS::LogLevel::FATAL, __FILE__, __LINE__ , 0, 1, 2, time(0), std::string("noname")));
    // event->getSS() << "hello sylar log";
    // std::cout << "ss content: " <<event->getSS().str() << std::endl;
    // logger->log(event->getLevel(), event);
    // //HPGS_WRITE_LOG(logger, event, "Hello sylar log");
    // std::cout << "End test" << std::endl;

    // HPGS_LOG_INFO(logger) << "test macro";
    // HPGS_LOG_ERROR(logger) << "test macro error";

    // HPGS_LOG_FMT_ERROR(logger, "test macro fmt error: %s", "aa");

    // auto l = HPGS::LoggerMgr::GetInstance()->getLogger("xx");
    // HPGS_LOG_INFO(l) << "xxx";

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

    //return 0;
}