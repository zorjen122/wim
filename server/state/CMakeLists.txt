cmake_minimum_required(VERSION 3.16)
project(State LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)

message(STATUS "Using protobuf ${Protobuf_VERSION}")
message(STATUS "Using gRPC ${gRPC_VERSION}")


set(_SPDLOG_LIB_DIR /usr/local/spdlog/lib)
set(_YAML_LIB_DIR /usr/local/yaml-cpp/lib/)


set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_HIREDIS /usr/include/hiredis)
set(_DIR_MYSQLCPPCONN /usr/include/mysql)
set(_DIR_SPDLOG /usr/local/spdlog/include)
set(_DIR_YAML /usr/local/yaml-cpp/include)


set(_LOCAL_DIR_INCLUDE ${CMAKE_SOURCE_DIR}/include)
set(_LOCAL_DIR_GRPC_INCLUDE ${CMAKE_SOURCE_DIR}/rpc)
set(_PUBLIC_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../public/include)


set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_REFLECTION gRPC::grpc++_reflection)
set(_JSONCPP jsoncpp)
set(_MYSQLCPPCONN mysqlcppconn)
set(_HIREDIS hiredis)
set(_SPDLOG spdlog)
set(_PROTOBUF protobuf::libprotobuf)
set(_YAML yaml-cpp)



file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/**/*.cc ${CMAKE_SOURCE_DIR}/src/*.cc)
file(GLOB RPC_SOURCES ${CMAKE_SOURCE_DIR}/rpc/*.cc)
file(GLOB PUBLIC_SOURCES ${CMAKE_SOURCE_DIR}/../public/src/*.cc)


add_executable(State 
        ${SOURCES}
        ${RPC_SOURCES}
        ${PUBLIC_SOURCES}
)

target_include_directories(State PUBLIC
    ${_DIR_JSONCPP}
    ${_DIR_HIREDIS}
    ${_DIR_MYSQLCPPCONN}
    ${_DIR_SPDLOG}
    ${_LOCAL_DIR_INCLUDE}
    ${_LOCAL_DIR_GRPC_INCLUDE}
    ${_PUBLIC_INCLUDE}
    ${_DIR_YAML}
)

target_link_directories(State PUBLIC
    ${_SPDLOG_LIB_DIR}
    ${_YAML_LIB_DIR}
)

target_link_libraries(State
    ${_GRPC_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF}
    ${_JSONCPP}
    ${_HIREDIS}
    ${_MYSQLCPPCONN}
    ${_SPDLOG}
    ${_YAML}
)

target_compile_options(State PRIVATE -Wall -g)

# error