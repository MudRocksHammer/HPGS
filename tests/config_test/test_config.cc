#include "config.h"
#include "log.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "env.h"
#include <gtest/gtest.h>

#if 1
HPGS::ConfigVar<int>::ptr g_int_value_config = 
    HPGS::Config::Lookup("system.port", (int)8000, "system port");

HPGS::ConfigVar<float>::ptr g_float_value_config = 
    HPGS::Config::Lookup("system.val", (float)10.2f, "system value");

void test_yaml(){
    YAML::Node root = YAML::LoadFile("/home/xiaorenbo/share/HPGS/conf/log.yml");

    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << root["test"].IsDefined();
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << root["logs"].IsDefined();
    HPGS_LOG_INFO(HPGS_LOG_ROOT()) << root;
}
#endif

class Person{
public:
    Person() {}
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name = " << m_name
            << "age = " << m_age
            << "sex = " << m_sex
            <<"]";
        return ss.str();
    }

    bool operator==(const Person& oth) const {
        return m_name == oth.m_name
                && m_age == oth.m_age
                && m_sex == oth.m_sex;
    }
};

namespace HPGS{

template<>
class LexicalCast<std::string, Person>{
public:
    Person operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};

template<>
class LexicalCast<Person, std::string>{
public:
    std::string operator()(const Person& p){
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

HPGS::ConfigVar<Person>::ptr g_person = 
    HPGS::Config::Lookup("class.person", Person(), "system person");

HPGS::ConfigVar<std::map<std::string, Person> >::ptr g_person_map = 
    HPGS::Config::Lookup("class.map", std::map<std::string, Person>(), "system person");

HPGS::ConfigVar<std::map<std::string, std::vector<Person> > >::ptr g_person_vec_map = 
    HPGS::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person> >(), "system person");

TEST(CONFIG_TEST, env_config){
    test_yaml();
}

int main(int argc, char* argv[]){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
    // HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->getValue();
    // HPGS_LOG_INFO(HPGS_LOG_ROOT()) << g_int_value_config->toString();
    // char buffer[1024];
    // strcpy(buffer, get_current_dir_name());

    // return 0;
}