#include "config.h"
#include "log.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "env.h"

#if 1
HPGS::ConfigVar<int>::ptr g_int_value_config = 
    HPGS::Config::Lookup("System.port", (int)8000, "system port");

HPGS::ConfigVar<float>::ptr g_float_value_config = 
    HPGS::Config::Lookup("system.port", (float)10.2f, "system value");

void test_yaml(){
    YAML::Node root = YAML::LoadFile("");
}
#endif

int main(int argc, char* argv[]){
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->getValue();
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->toString();
    char buffer[1024];
    strcpy(buffer, get_current_dir_name());

    return 0;
}