cmake_minimum_required(VERSION 3.16)
project(state LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 依赖库
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
set(_INCLUDE ./include)
set(_PUBLIC_INCLUDE ./../public/include)
set(_RPC_INCLUDE ./rpc/)


file(GLOB SOURCES 
    ${CMAKE_SOURCE_DIR}/src/**/*.cc 
    ${CMAKE_SOURCE_DIR}/src/*.cc
)
file(GLOB RPC_SOURCES ${CMAKE_SOURCE_DIR}/rpc/*.cc)

add_executable(state 
        ${SOURCES}
        ${RPC_SOURCES})

target_include_directories(state PRIVATE
    ${_INCLUDE}
    ${_PUBLIC_INCLUDE}
    ${_RPC_INCLUDE}
)

target_link_directories(state PUBLIC ../public/build)

# 库名称
set(_PUBLIC imPublic)
set(_GRPC gRPC::grpc++ gRPC::grpc++_reflection)
set(_PROTOBUF protobuf::libprotobuf)
set(_BOOST Boost::filesystem Boost::system)
set(_JSONCPP jsoncpp)
set(_MYSQLX8 mysqlcppconn8)
set(_REDIS hiredis redis++)
set(_SPDLOG spdlog)
set(_YAML yaml-cpp)
set(_RDKAFKA rdkafka++ rdkafka)


target_link_libraries(state
    ${_PUBLIC}
    ${_GRPC}
    ${_PROTOBUF}
    ${_JSONCPP}
    ${_REDIS}
    ${_MYSQLX8}
    ${_BOOST}
    ${_SPDLOG}
    ${_YAML}
    ${_RDKAFKA}
)


target_compile_options(state PRIVATE -Wall -g)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()