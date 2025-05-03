cmake_minimum_required(VERSION 3.1)
project(imTest LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

set(protobuf_MODULE_COMPATIBLE TRUE)
file(GLOB SOURCE src/service/*.cc)

add_executable(imTest ${SOURCE})

target_include_directories(imTest PUBLIC ../public/include ./include)
target_link_directories(imTest PUBLIC ../public/build)

target_link_libraries(
  imTest
  PUBLIC imPublic
         Threads::Threads
         protobuf::libprotobuf
         Boost::system
         Boost::filesystem
         yaml-cpp
         spdlog
         hiredis
         redis++
         mysqlcppconn8
         jsoncpp
         rdkafka
         rdkafka++)

# find_package(GTest REQUIRED) include(CTest)

# function(add_service_test target) add_executable(${target} ${ARGN} $)
# target_link_libraries(${target} PRIVATE GTest::GTest GTest::Main imTest)

# add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target}) endfunction()

# add_service_test(test_chat_service src/cmd/test_chat_service.cc)
