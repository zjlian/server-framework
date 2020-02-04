cmake_minimum_required(VERSION 3.15)
project(server_framework)

# SET(CMAKE_C_COMPILER "/usr/bin/gcc-9")
# SET(CMAKE_CXX_COMPILER "/usr/bin/g++-9")
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++14 -Wall -Wno-deprecated -Werror -Wno-unused-function")
set(CMAKE_CXX_STANDARD 17)

# 引入boost
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.58.0 COMPONENTS REQUIRED)
if(Boost_FOUND)

    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})

endif()

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_library(${PROJECT_NAME} SHARED src/log.cpp src/log.h src/util.cpp src/util.h src/config.cpp src/config.h)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} yaml-cpp)

add_executable(test ${PROJECT_SOURCE_DIR}/tests/test.cc)
add_dependencies(test ${PROJECT_NAME})
target_link_libraries(test ${PROJECT_NAME})

add_executable(test_config ${PROJECT_SOURCE_DIR}/tests/test_config.cc)
add_dependencies(test_config ${PROJECT_NAME})
target_link_libraries(test_config ${PROJECT_NAME})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
