cmake_minimum_required(VERSION 3.16)
project(gate LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(gRPC REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
wim_generate_grpc_proto(VERIFY_PROTO ../verify/message.proto)

file(GLOB SOURCE src/**/*.cc)
add_library(imGate ${SOURCE} ${VERIFY_PROTO})
add_executable(gate src/main.cc)

target_include_directories(imGate PUBLIC ./include ${VERIFY_PROTO_INCLUDE_DIR}
                                        ../public/include)
target_link_directories(imGate PUBLIC ../public/build)

target_link_libraries(
  imGate
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
         jsoncpp)

target_compile_options(imGate PRIVATE -Wall -g)

target_link_libraries(gate PRIVATE imGate)

target_compile_options(gate PRIVATE -Wall)

if(WIM_BUILD_UNIT_TESTS)
  add_subdirectory(test)
endif()
