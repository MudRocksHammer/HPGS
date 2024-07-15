#include "fibre.h"
#include "log.h"
#include "thread.h"
#include <string>
#include <gtest/gtest.h>
#include <chrono>
#include <random>

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

HPGS::Mutex s_mutex;

bool running = true;

void run_in_fibre(){
    HPGS_LOG_INFO(g_logger) << "run_in_fiber begin";
    HPGS::Fibre::YieldToHold();
    HPGS_LOG_INFO(g_logger) << "run_in_fibre end";
    HPGS::Fibre::YieldToHold();
}

//一个函数只执行一次
void fibre_fun(const std::string& content){
    while(running){
        {
            HPGS::Mutex::Lock lock(s_mutex);
            std::cout << "child fibre --" + content + ":" + std::to_string(HPGS::GetThreadId()) + "  " + std::to_string(HPGS::GetFibreId()) << std::endl;
        }
        HPGS::Fibre::YieldToHold();
    }
}

void th_fun(){
    HPGS_LOG_INFO(g_logger) << "th start";

    HPGS::Fibre::createMainFibre();

    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    std::vector<HPGS::Fibre::ptr> fibres;

    int i = 0;
    for(; i < 3; i++){
        std::string str = "some random message that will be send-->" + std::to_string(i) + " ";
        fibres.push_back(std::make_shared<HPGS::Fibre>(std::bind(fibre_fun, str)));
    }
    
    //线程执行5秒
    while(std::chrono::steady_clock::now() < end_time){
        std::random_device rd;
        std::mt19937 gen(rd()); // 标准的mersenne_twister_engine
        // 创建均匀分布,范围从0到500
        std::uniform_int_distribution<> dis(0, 500);
        uint32_t tem = dis(gen);
        (++i %= 3);
        
        fibres[i]->swapIn();

        auto next_time = std::chrono::steady_clock::now();
        
        while(std::chrono::steady_clock::now() < next_time + std::chrono::milliseconds(tem)){}
    }

    running = false;
    
    for(i = 0; i < 3; i++){
        fibres[i]->swapIn();
    }
}

void test_fibre(){
    //在线程里面手动调度协程
    HPGS_LOG_INFO(g_logger) << "main begin -1";
    {
        HPGS::Fibre::GetThis();
        HPGS_LOG_INFO(g_logger) << "main begin";
        //创建一个fibre开始执行
        HPGS::Fibre::ptr fibre(new HPGS::Fibre(std::bind(fibre_fun, " cool")));
        fibre->swapIn();
        HPGS_LOG_INFO(g_logger) << "main after swapIn";
        fibre->swapIn();
        HPGS_LOG_INFO(g_logger) << "main after end";
        fibre->swapIn();
        //fibre->~Fibre();
    }
    HPGS_LOG_INFO(g_logger) << "main fater end2";
}

TEST(test_fibre, fibre1){
    HPGS::Thread::SetName("main");

    std::vector<HPGS::Thread::ptr> threads;
    // for(int i = 0; i < 1; i++){
    //     threads.push_back(HPGS::Thread::ptr(new HPGS::Thread(&test_fibre, "name_" + std::to_string(i))));
    // }
    for(int i = 0; i < 3; i++){
        threads.push_back(HPGS::Thread::ptr(new HPGS::Thread(&th_fun, "name_" + std::to_string(i))));
    }
    for(auto i : threads){
        i->join();
    }
    HPGS_LOG_INFO(g_logger) << "end test";
}

int main(int argc, char* argv[]){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}