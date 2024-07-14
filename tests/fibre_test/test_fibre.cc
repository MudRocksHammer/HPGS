#include "fibre.h"
#include "log.h"
#include "thread.h"
#include <string>
#include <gtest/gtest.h>

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

void run_in_fibre(){
    HPGS_LOG_INFO(g_logger) << "run_in_fiber begin";
    HPGS::Fibre::YieldToHold();
    HPGS_LOG_INFO(g_logger) << "run_in_fibre end";
    HPGS::Fibre::YieldToHold();
}

void test_fibre(){
    HPGS_LOG_INFO(g_logger) << "main begin -1";
    {
        HPGS::Fibre::GetThis();
        HPGS_LOG_INFO(g_logger) << "main begin";
        HPGS::Fibre::ptr fibre(new HPGS::Fibre(run_in_fibre));
        fibre->swapIn();
        HPGS_LOG_INFO(g_logger) << "main after swapIn";
        fibre->swapIn();
        HPGS_LOG_INFO(g_logger) << "main after end";
        fibre->swapIn();
    }
    HPGS_LOG_INFO(g_logger) << "main fater end2";
}

TEST(test_fibre, fibre1){
    HPGS::Thread::SetName("main");

    std::vector<HPGS::Thread::ptr> threads;
    for(int i = 0; i < 3; i++){
        threads.push_back(HPGS::Thread::ptr(new HPGS::Thread(&test_fibre, "name_" + std::to_string(i))));
    }
    for(auto i : threads){
        i->join();
    }
}

int main(int argc, char* argv[]){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}