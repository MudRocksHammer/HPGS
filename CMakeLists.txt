cmake_minimum_required(VERSION 3.10)
project(HPGS)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

find_package(yaml-cpp REQUIRED)
find_package(jsoncpp REQUIRED)

file(GLOB DIR_SRC "src/*.cc")

set(LIBS yaml-cpp::yaml-cpp
         jsoncpp
         pthread
)

add_library(HPGS SHARED ${DIR_SRC})
#add_library(HPGS_static static ${LIB_SRC})
#SET_TARGET_PROPERTIES (HPGS_static PROPERTIES OUTPUT_NAME "HPGS")
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(HPGS PRIVATE ${LIBS})

enable_testing()
add_subdirectory(tests)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
