cmake_minimum_required(VERSION 3.1)
project(wim_system VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 为工具链提供编译数据库

option(BUILD_TESTING "Build tests" ON)
option(BUILD_DEMOS "Build demo programs" OFF)

add_subdirectory(public)
add_subdirectory(chat)
add_subdirectory(gate)
add_subdirectory(state)

if(BUILD_TESTING)
    add_subdirectory(test) 
endif()
