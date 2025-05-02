cmake_minimum_required(VERSION 3.10)

project(public_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include(CTest)

function(add_public_test target)
  add_executable(${target} ${ARGN})
  target_include_directories(${target} PUBLIC include)
  target_link_libraries(${target} PRIVATE GTest::GTest GTest::Main imPublic)

  add_test(NAME ${CMAKE_PROJECT_NAME}.${target} COMMAND ${target})
endfunction()

add_public_test(test_mysql_dao mysqlDaoTest.cc)
add_public_test(test_redis_dao redisDaoTest.cc)
