cmake_minimum_required (VERSION 2.6)
project(mhwd)

set(EXECUTABLE_OUTPUT_PATH ${mhwd_SOURCE_DIR}/bin)

set(CMAKE_INSTALL_PREFIX "/usr/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -Wall -Wextra -Werror")

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code")
endif()

add_subdirectory(libmhwd)
add_subdirectory(src)
add_subdirectory(scripts)
