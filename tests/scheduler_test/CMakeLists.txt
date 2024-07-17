find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_scheduler test_scheduler.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_scheduler ${LIBS})

add_test(NAME SCHEDULER_TEST COMMAND test_scheduler)