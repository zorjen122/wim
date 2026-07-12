cmake_minimum_required(VERSION 3.16)
project(state LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
wim_generate_grpc_proto(STATE_PROTO ../public/proto/state.proto)
wim_generate_grpc_proto(IM_PROTO ../public/proto/im.proto)

file(GLOB SOURCE src/**/*.cc)
add_library(imState ${SOURCE} ${STATE_PROTO} ${IM_PROTO})
add_executable(state src/main.cc)

target_include_directories(imState PUBLIC ./include ${STATE_PROTO_INCLUDE_DIR}
                                         ${IM_PROTO_INCLUDE_DIR}
                                         ../public/include)
target_link_directories(imState PUBLIC ../public/build)

target_link_libraries(
  imState
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
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

target_compile_options(imState PRIVATE -Wall -g)

target_link_libraries(state PRIVATE imState)

target_compile_options(state PRIVATE -Wall)

if(WIM_BUILD_UNIT_TESTS)
  add_subdirectory(test)
endif()
