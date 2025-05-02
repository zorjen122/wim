cmake_minimum_required(VERSION 3.10)
project(imPublic)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(protobuf_MODULE_COMPATIBLE TRUE)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)  
find_package(gRPC CONFIG REQUIRED)

message(STATUS "Using Threads ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "Using Boost ${Boost_VERSION}")
message(STATUS "Using protobuf ${Protobuf_VERSION}")
message(STATUS "Using gRPC ${gRPC_VERSION}")

# 第三方库头文件
set(_INCLUDE include/)

# 添加 src 目录下的所有源文件
file(GLOB SOURCES src/*.cc)

add_library(imPublic STATIC ${SOURCES})

target_include_directories(imPublic PUBLIC
    ${_INCLUDE}
)

# 单元测试
if(BUILD_TESTING)
    add_subdirectory(test/db)
endif()

