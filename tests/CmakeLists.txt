include_directories(../src)

file(GLOB CPP_SRC_LIST *.cc)

foreach(v ${CPP_SRC_LIST})
    string(REGEX MATCH "tests/.*" relative_path ${v})
    string(REGEX REPLACE "tests/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cc" "" target_name ${target_name})
    message(STATUS "找到测试文件：${v}")
    add_executable(${target_name} ${v})
    target_link_libraries(${target_name} libconet)
endforeach()
