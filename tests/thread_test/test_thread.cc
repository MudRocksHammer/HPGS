#include "thread.h"
#include <gtest/gtest.h>
#include "mutex.h"
#include "log.h"
#include "util.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <mutex>

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

int count = 0;

HPGS::Mutex s_mutex;

std::mutex fileMutex;

void writeFile(std::ofstream& ofs, const std::string& content){
    for(int i = 0; i < 10000; i++){
        HPGS::Mutex::Lock lock(s_mutex);
        //std::lock_guard<std::mutex> lock(fileMutex);
        if (!ofs){
            std::cerr << "Error opening file for writing" << std::endl;
            return;
        }
        ofs << content + " -- " + std::to_string(i) + "\n";
    }    
}

void fun1(){
    HPGS_LOG_INFO(g_logger) << "name: " << HPGS::Thread::GetName()
                            << " this.name: " << HPGS::Thread::GetThis()->getName()
                            << " id: " << HPGS::GetThreadId()
                            << " this.id: " << HPGS::Thread::GetThis()->getId();
    
    for(int i = 0; i < 100000; i++){
        HPGS::Mutex::Lock lock(s_mutex);
        ++count;
    }
}

void fun2(){
    while(true){
        HPGS_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void fun3(){
    while(true){
        HPGS_LOG_INFO(g_logger) << "========================================================";
    }
}

int main(int argc, char* argv[]){
    HPGS_LOG_INFO(g_logger) << "thread test begin";
    YAML::Node root = YAML::LoadFile("/home/xiaorenbo/share/HPGS/conf/log.yml");

    std::ofstream ofs("test.txt", std::ios::out | std::ios::app | std::ios::ate);

    std::vector<HPGS::Thread::ptr> threads;
    std::string str = "write some words ";
    for(int i = 0; i < 3; i++){
        HPGS::Thread::ptr thread(new HPGS::Thread(std::bind(writeFile, std::ref(ofs), str + std::to_string(i)), "name_" + std::to_string(i * 2)));
        threads.push_back(thread);
    }

    for(size_t i = 0; i < threads.size(); i++){
        threads[i]->join();
    }

    HPGS_LOG_INFO(g_logger) << "thread test end";
    HPGS_LOG_INFO(g_logger) << "count = " << count;

    ofs.close();
    return 0;
}