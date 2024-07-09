find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_log test_log.cc)

target_link_libraries(test_log HPGS ${GTEST_LIBRARIES} pthread)

add_test(NAME LOG_TEST COMMAND test_log)