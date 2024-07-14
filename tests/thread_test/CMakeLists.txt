find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_thread test_thread.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_thread ${LIBS})

add_test(NAME THREAD_TEST COMMAND test_thread)