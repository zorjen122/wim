cmake_minimum_required(VERSION 3.1)
project(chat LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem coroutine)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/**/*.cc)
file(GLOB RPC_SOURCE ../public/rpc/chat/*.cc ../public/rpc/file/*.cc)
add_library(imChat ${SOURCE} ${RPC_SOURCE})
add_executable(chat src/main.cc)

target_include_directories(imChat PUBLIC ./include ../public/rpc/chat
                                         ../public/rpc/file ../public/include)
target_link_directories(imChat PUBLIC ../public/build)

target_link_libraries(
  imChat
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::system
         Boost::filesystem
         Boost::coroutine
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

target_compile_options(imChat PRIVATE -Wall -g)

target_link_libraries(chat PRIVATE imChat)

target_compile_options(chat PRIVATE -Wall -g)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
