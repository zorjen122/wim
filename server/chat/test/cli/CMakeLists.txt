cmake_minimum_required(VERSION 3.10)

project(chat_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(GTest REQUIRED)
include(CTest)

function(add_chat_test target)
  add_executable(${target} ${ARGN})
  target_include_directories(${target} PUBLIC include ../../public/include)
  target_link_directories(${target} PUBLIC ../../public/build)
  target_link_libraries(
    ${target}
    PRIVATE GTest::GTest
            GTest::Main
            imPublic
            Threads::Threads
            protobuf::libprotobuf
            Boost::system
            yaml-cpp
            spdlog
            mysqlcppconn8
            jsoncpp)

  add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target})
endfunction()

add_chat_test(test_demo src/demo.cc)
