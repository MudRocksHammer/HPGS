find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_config test_config.cc)

find_package(yaml-cpp REQUIRED)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_config ${LIBS})

add_test(NAME CONFIG_TEST COMMAND test_config)