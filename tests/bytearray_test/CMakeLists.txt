find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_bytearray test_bytearray.cc)

find_package(yaml-cpp REQUIRED)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_bytearray ${LIBS})

add_test(NAME BYTEARRAY_TEST COMMAND test_bytearray)