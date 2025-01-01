cmake_minimum_required(VERSION 3.16)
project(ChatServer LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)  

# protobuf link
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)


set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_HIREDIS /usr/include/hiredis)
set(_DIR_MYSQLCPPCONN /usr/include/)
set(_LOCAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(_BOOST Boost::filesystem Boost::system)
set(_JSONCPP jsoncpp)
set(_MYSQLCPPCONN mysqlcppconn)
set(_HIREDIS hiredis)

# 添加可执行文件和源文件
file(GLOB SOURCES_SERVICE ${CMAKE_CURRENT_SOURCE_DIR}/src/service/*.cpp)
file(GLOB SOURCES_UTIL ${CMAKE_CURRENT_SOURCE_DIR}/src/util/*.cpp)
file(GLOB PBSOURCES ${CMAKE_CURRENT_SOURCE_DIR}/rpc/*.cc)


# TODO, 启用asio中io_uring选项
# add_definitions(-DASIO_HAS_IO_URING)
# add_definitions(-DASIO_DISABLE_EPOLL)

add_executable(ChatServer 
        ${SOURCES_SERVICE}
        ${PBSOURCES}
        ${SOURCES_UTIL})

target_include_directories(ChatServer PUBLIC
    ${_DIR_JSONCPP}
    ${_DIR_HIREDIS}
    ${_DIR_MYSQLCPPCONN}
    ${_LOCAL_INCLUDE_DIR}
)

target_link_libraries(ChatServer
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${_JSONCPP}
    ${_HIREDIS}
    ${_MYSQLCPPCONN}
    ${_BOOST}
)

target_compile_options(ChatServer PRIVATE -Wall -g)


# error