cmake_minimum_required(VERSION 3.16)
project(GateServer LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)  

# protobuf link
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)
set(_REFLECTION gRPC::grpc++_reflection)


set(_SPDLOG_LIB_DIR /usr/local/spdlog/lib)


set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_HIREDIS /usr/include/hiredis)
set(_DIR_MYSQLCPPCONN /usr/include/mysql/)
set(_DIR_SPDLOG /usr/local/spdlog/include)

set(_LOCAL_DIR_INCLUDE ${CMAKE_SOURCE_DIR}/include)
set(_LOCAL_DIR_GRPC_INCLUDE ${CMAKE_SOURCE_DIR}/rpc)

set(_BOOST Boost::filesystem Boost::system)
set(_JSONCPP jsoncpp)
set(_MYSQLCPPCONN mysqlcppconn)
set(_HIREDIS hiredis)
set(_SPDLOG spdlog)




# 添加可执行文件和源文件
file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/**/*.cc)
file(GLOB PBSOURCES ${CMAKE_SOURCE_DIR}/rpc/*.cc)

add_executable(GateServer ${SOURCES}
        ${PBSOURCES})

target_include_directories(GateServer PUBLIC
    ${_DIR_JSONCPP}
    ${_DIR_HIREDIS}
    ${_DIR_MYSQLCPPCONN}
    ${_DIR_SPDLOG}
    ${_LOCAL_DIR_INCLUDE}
    ${_LOCAL_DIR_GRPC_INCLUDE}
)

target_link_directories(GateServer PUBLIC
    ${_SPDLOG_LIB_DIR}
)

target_link_libraries(GateServer
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${_JSONCPP}
    ${_HIREDIS}
    ${_MYSQLCPPCONN}
    ${_BOOST}
    ${_SPDLOG}
)

target_compile_options(GateServer PRIVATE -Wall -g)

# error