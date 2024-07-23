cmake_minimum_required(VERSION 3.10)
project(HPGS)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
#set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

find_package(yaml-cpp REQUIRED)
find_package(jsoncpp REQUIRED)

file(GLOB_RECURSE DIR_SRC "${CMAKE_SOURCE_DIR}/src/*.cc")

set(LIBS yaml-cpp::yaml-cpp
         jsoncpp
         pthread
         dl
)

add_library(HPGS SHARED ${DIR_SRC})
#add_library(HPGS_static static ${LIB_SRC})
#SET_TARGET_PROPERTIES (HPGS_static PROPERTIES OUTPUT_NAME "HPGS")

file(GLOB_RECURSE H_FILES "${CMAKE_SOURCE_DIR}/include/*.h" )
set(DIRLIST "")
foreach(file ${H_FILES})
    get_filename_component(dir ${file} DIRECTORY)
    list(APPEND DIRLIST ${dir})
endforeach()
list(REMOVE_DUPLICATES DIRLIST)

include_directories(${DIRLIST})

target_link_libraries(HPGS PRIVATE ${LIBS})

enable_testing()
add_subdirectory(tests)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
