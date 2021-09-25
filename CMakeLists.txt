cmake_minimum_required (VERSION 3.12)
project(mhwd)

set(EXECUTABLE_OUTPUT_PATH ${mhwd_SOURCE_DIR}/bin)

set(CXX_STANDARD 20)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code")
endif()

add_subdirectory(libmhwd)
add_subdirectory(src)
add_subdirectory(scripts)
