find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_iomanager test_iomanager.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_iomanager ${LIBS})

add_test(NAME IOMANAGER_TEST COMMAND test_iomanager)