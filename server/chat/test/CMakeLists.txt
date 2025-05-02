cmake_minimum_required(VERSION 3.10)

project(chat_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include(CTest)

function(add_chat_test target)
  add_executable(${target} ${ARGN})
  target_include_directories(${target} PUBLIC include)
  target_link_libraries(${target} PRIVATE GTest::GTest GTest::Main imChat)

  add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target})
endfunction()

add_chat_test(test_chat_session TestChatSession.cc)
add_chat_test(test_demo demo.cc)
