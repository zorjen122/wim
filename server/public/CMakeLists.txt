cmake_minimum_required(VERSION 3.10)

project(imPublic)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(protobuf_MODULE_COMPATIBLE TRUE)

# 仅用于clangd搜索
find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(gRPC REQUIRED)

# 添加 src 目录下的所有源文件
file(GLOB SOURCES src/*.cc)

add_library(imPublic SHARED ${SOURCES})

# add_library(imPublicStatic STATIC ${SOURCES})

target_include_directories(imPublic PUBLIC ./include)
target_link_libraries(
  imPublic
  PUBLIC Threads::Threads
         protobuf::libprotobuf
         Boost::system
         Boost::filesystem
         gRPC::grpc++
         gRPC::grpc++_reflection
         yaml-cpp
         spdlog
         jsoncpp
         hiredis
         redis++
         mysqlcppconn8)

# 单元测试
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
