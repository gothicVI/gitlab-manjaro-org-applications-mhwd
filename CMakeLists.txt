cmake_minimum_required (VERSION 3.12)
project(mhwd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code")
endif()

add_subdirectory(libmhwd)
add_subdirectory(src)
add_subdirectory(scripts)
