find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_fibre test_fibre.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
)

target_link_libraries(test_fibre ${LIBS})

add_test(NAME FIBRE_TEST COMMAND test_fibre)