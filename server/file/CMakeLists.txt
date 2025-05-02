cmake_minimum_required(VERSION 3.1)
project(file LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/**/*.cc)
file(GLOB RPC_SOURCE rpc/*.cc)
add_library(imFile ${SOURCE} ${RPC_SOURCE})
add_executable(file src/main.cc)

target_include_directories(imFile PUBLIC ./include ./rpc ../public/include)
target_link_directories(imFile PUBLIC ../public/build)

target_link_libraries(
  imFile
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         imPublic
         gRPC::grpc++
         gRPC::grpc++_reflection
         spdlog
         yaml-cpp
         Threads::Threads)

target_compile_options(imFile PRIVATE -Wall -g)

target_link_libraries(file PRIVATE imFile)

target_compile_options(file PRIVATE -Wall)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
