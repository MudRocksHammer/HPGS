cmake_minimum_required(VERSION 3.10)
project(HPGS)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -03 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

set(LIB_SRC HPGS/loc.cc)

add_library(HPGS SHARED ${LIB_SRC})
#add_library(HPGS_static static ${LIB_SRC})
#SET_TARGET_PROPERTIES (HPGS_static PROPERTIES OUTPUT_NAME "HPGS")

add_executable(test tests/test.cc)
add_dependencies(test HPGS)
target_link_libraries(test HPGS)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)