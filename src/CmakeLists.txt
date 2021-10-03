# 引入boost
find_package(Boost COMPONENTS REQUIRED)
if (Boost_FOUND)

    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})

endif ()

# file(GLOB CPP_SRC_LIST *.cc)
aux_source_directory(. CPP_SRC_LIST)

add_library(libconet STATIC ${CPP_SRC_LIST})

target_link_libraries(libconet ${Boost_LIBRARIES} yaml-cpp pthread dl)