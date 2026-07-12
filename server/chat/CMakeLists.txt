cmake_minimum_required(VERSION 3.16)
project(chat LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)

file(GLOB SOURCE src/**/*.cc)
add_library(imChat ${SOURCE})
add_executable(chat src/main.cc)

target_include_directories(imChat PUBLIC ./include ../public/include)
target_link_directories(imChat PUBLIC ../public/build)

target_link_libraries(
  imChat
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::filesystem
         gRPC::grpc++
         gRPC::grpc++_reflection
         fmt::fmt
         yaml-cpp
         spdlog
         hiredis
         redis++
         mysqlcppconnx
         jsoncpp
         rdkafka
         rdkafka++)

target_compile_options(imChat PRIVATE -Wall -g)

target_link_libraries(chat PRIVATE imChat)

target_compile_options(chat PRIVATE -Wall)

if(WIM_BUILD_UNIT_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
  add_subdirectory(test)
endif()
