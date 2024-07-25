find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

add_executable(test_socket test_socket.cc)

set(LIBS yaml-cpp::yaml-cpp
         pthread
         ${GTEST_LIBRARIES}
         HPGS
         ${OPENSSL_LIBRARIES}
)

target_link_libraries(test_socket ${LIBS})

add_test(NAME SOCKET_TEST COMMAND test_socket)