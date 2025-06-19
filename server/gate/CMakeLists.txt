cmake_minimum_required(VERSION 3.1)
project(gate LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem coroutine)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/*.cc)
file(GLOB RPC_SOURCE ../public/rpc/state/*.cc)
add_executable(gate src/main.cc)

add_library(imGate ${SOURCE} ${RPC_SOURCE})

target_include_directories(imGate PUBLIC ./include ../public/rpc/state
                                         ../public/include)
target_link_directories(imGate PUBLIC ../public/build)

target_link_libraries(
  imGate
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::system
         Boost::coroutine
         Boost::filesystem
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

target_compile_options(imGate PRIVATE -Wall -g)

target_link_libraries(gate PRIVATE imGate)

target_compile_options(gate PRIVATE -Wall)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
