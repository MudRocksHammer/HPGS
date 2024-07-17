#include "scheduler.h"
#include "log.h"

static HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

void fibre_func(){
    static int s_count = 5;
    HPGS_LOG_INFO(g_logger) << " test in fibre s_count = " << s_count;

    sleep(1);
    if(--s_count >= 0){
        HPGS::Scheduler::GetThis()->schedule(&fibre_func, HPGS::GetThreadId());
    }
}

int main(int argc, char* argv[]){
    HPGS_LOG_INFO(g_logger) << "main";
    HPGS::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    HPGS_LOG_INFO(g_logger) << "schedule";
    sc.schedule(&fibre_func);
    sc.stop();
    HPGS_LOG_INFO(g_logger) << "over";
    return 0;
}