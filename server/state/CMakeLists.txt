cmake_minimum_required(VERSION 3.1)
project(state LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/**/*.cc)
file(GLOB RPC_SOURCE rpc/*.cc)
add_library(imState ${SOURCE} ${RPC_SOURCE})
add_executable(state src/main.cc)

target_include_directories(imState PUBLIC ./include ./rpc ../public/include)
target_link_directories(imState PUBLIC ../public/build)

target_link_libraries(
  imState
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         gRPC::grpc++
         gRPC::grpc++_reflection
         yaml-cpp
         spdlog
         hiredis
         redis++
         mysqlcppconn8
         jsoncpp
         rdkafka
         rdkafka++)

target_compile_options(imState PRIVATE -Wall -g)

target_link_libraries(state PRIVATE imState)

target_compile_options(state PRIVATE -Wall)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
