cmake_minimum_required(VERSION 3.1)

project(TestService LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)  

set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_SPDLOG /usr/local/spdlog/include)
set(_DIR_MYSQLCPPCONN /usr/include/)

set(_LIB_SPDLOG_DIR /usr/local/spdlog/lib/)

set(_LIB_BOOST Boost::filesystem Boost::system)
set(_LIB_JSONCPP jsoncpp)
set(_LIB_PTHREAD pthread)
set(_LIB_SPDLOG spdlog)
set(_LIB_MYSQLCPPCONN mysqlcppconn)

# 搜索 util 和 service 目录下的所有 .cc 文件
file(GLOB UTIL_CPP ${CMAKE_CURRENT_SOURCE_DIR}/util/*.cc)
file(GLOB SERVICE_CPP ${CMAKE_CURRENT_SOURCE_DIR}/service/*.cc)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/util)

# 遍历 SERVICE_CPP 中的每个 .cc 文件
foreach(SRC_FILE ${SERVICE_CPP})
    # 获取文件名，不带路径和扩展名
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    # 创建一个新的可执行文件，每个 .cc 文件对应一个可执行文件
    add_executable(${EXEC_NAME} ${UTIL_CPP} ${SRC_FILE})

   # 设置每个可执行文件的输出目录为 ./bin
    set_target_properties(${EXEC_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # 设置每个可执行文件的包含目录
    target_include_directories(${EXEC_NAME} PUBLIC
        ${_DIR_JSONCPP}
        ${_DIR_SPDLOG}
        ${_DIR_MYSQLCPPCONN}
    )

    target_link_directories(${EXEC_NAME} PUBLIC ${_LIB_SPDLOG_DIR})

    # 设置每个可执行文件的链接库
    target_link_libraries(${EXEC_NAME}
        ${_LIB_BOOST}
        ${_LIB_JSONCPP}
        ${_LIB_SPDLOG}
        ${_LIB_PTHREAD}
        ${_LIB_MYSQLCPPCONN}
    )

    # 添加编译选项
    target_compile_options(${EXEC_NAME} PRIVATE -Wall -g)
endforeach()
