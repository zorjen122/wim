cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
   AND "$ENV{CXX}" STREQUAL ""
   AND NOT DEFINED CMAKE_CXX_COMPILER
   AND EXISTS "/usr/bin/clang++")
  set(CMAKE_CXX_COMPILER
      clang++
      CACHE FILEPATH "C++ compiler for standalone server/test builds")
endif()

project(imTest LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(fmt REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/service/*.cc)

if(NOT TARGET imPublic)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../public"
                   "${CMAKE_CURRENT_BINARY_DIR}/public")
endif()

add_executable(imTest ${SOURCE})
add_executable(imPerf src/perf/im_perf.cc)

target_include_directories(
  imTest PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../public/include"
                "${CMAKE_CURRENT_LIST_DIR}/include")

target_include_directories(
  imPerf PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../public/include"
                "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(
  imTest
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::filesystem
         fmt::fmt
         yaml-cpp
         spdlog
         hiredis
         redis++
         mysqlcppconnx
         jsoncpp
         rdkafka
         rdkafka++)

target_link_libraries(
  imPerf
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::filesystem
         fmt::fmt)

# find_package(GTest REQUIRED) include(CTest)

# function(add_service_test target) add_executable(${target} ${ARGN} $)
# target_link_libraries(${target} PRIVATE GTest::GTest GTest::Main imTest)

# add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target}) endfunction()

# add_service_test(test_chat_service src/cmd/test_chat_service.cc)
