find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_hook test_hook.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_hook ${LIBS})

add_test(NAME HOOK_TEST COMMAND test_hook)