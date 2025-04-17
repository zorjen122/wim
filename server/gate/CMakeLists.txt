cmake_minimum_required(VERSION 3.16)
project(Gate LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)  
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

message(STATUS "Using Threads ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "Using Boost ${Boost_VERSION}")
message(STATUS "Using protobuf ${Protobuf_VERSION}")
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(protobuf_MODULE_COMPATIBLE TRUE)

# 本地头文件
set(_LOCAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(_RPC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rpc)
set(_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../public/include)

# 第三方库头文件
set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_HIREDIS /usr/include/hiredis)
set(_DIR_MYSQLCPPCONN /usr/include/)
set(_DIR_YAML_CPP_DIR /usr/local/include/)
set(_DIR_SPDLOG /usr/local/spdlog/include)

# 第三方库链接目录
set(_LIB_SPDLOG_DIR /usr/local/spdlog/lib/)
set(_LIB_YAML_CPP_DIR /usr/local/yaml-cpp/lib/)

# 库名称
set(_GRPC_GRPCPP gRPC::grpc++)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_BOOST Boost::filesystem Boost::system)
set(_JSONCPP jsoncpp)
set(_MYSQLCPPCONN mysqlcppconn8)
set(_HIREDIS hiredis redis++)
set(_SPDLOG spdlog)
set(_YAML yaml-cpp)


file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/**/*.cc )
file(GLOB RPC_SOURCES ${CMAKE_SOURCE_DIR}/rpc/*.cc)
file(GLOB PUBLIC_SOURCES ${CMAKE_SOURCE_DIR}/../public/src/*.cc)

add_executable(Gate
        ${CMAKE_SOURCE_DIR}/src/main.cc
        ${SOURCES}
        ${RPC_SOURCES}
        ${PUBLIC_SOURCES})

target_include_directories(Gate PUBLIC
    ${_DIR_JSONCPP}
    ${_DIR_HIREDIS}
    ${_DIR_MYSQLCPPCONN}
    ${_LOCAL_INCLUDE_DIR}
    ${_PUBLIC_INCLUDE_DIR}
    ${_RPC_INCLUDE_DIR}
    ${_DIR_SPDLOG}
    ${_DIR_YAML_CPP_DIR}
)

target_link_directories(Gate PUBLIC 
    ${_LIB_SPDLOG_DIR}  
    ${_LIB_YAML_CPP_DIR}
)


target_link_libraries(Gate
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${_JSONCPP}
    ${_HIREDIS}
    ${_MYSQLCPPCONN}
    ${_BOOST}
    ${_SPDLOG}
    ${_YAML}
)

target_compile_options(Gate PRIVATE -std=c++17 -Wall -g)
