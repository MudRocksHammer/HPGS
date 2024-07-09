find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_config test_config.cc)

target_link_libraries(test_config HPGS ${GTEST_LIBRARIES} pthread)

add_test(NAME CONFIG_TEST COMMAND test_config)