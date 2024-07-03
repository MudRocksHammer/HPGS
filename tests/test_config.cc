#include "include/config.h"
#include "include/log.h"


HPGS::ConfigVar<int>::ptr g_int_value_config = HPGS::Config::Lookup("system.port", (int)8000, "system port");

int main(int argc, char* argv[]){
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->getValue();
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->toString();

    return 0;
}