cmake_minimum_required(VERSION 3.10)

project(state_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include(CTest)

function(add_state_test target)
  add_executable(${target} ${ARGN})
  target_include_directories(${target} PUBLIC include)
  target_link_libraries(${target} PRIVATE GTest::GTest GTest::Main imState)

  add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target})
endfunction()

add_state_test(test_demo demo.cc)
