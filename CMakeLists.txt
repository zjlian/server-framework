cmake_minimum_required(VERSION 3.15)
project(server_framework)

# SET(CMAKE_C_COMPILER "/usr/bin/gcc-9")
# SET(CMAKE_CXX_COMPILER "/usr/bin/g++-9")
set(CMAKE_VERBOSE_MAKEFILE ON)
add_definitions("-O0 -g -ggdb -Wno-unused-variable")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(src)
add_subdirectory(tests)

