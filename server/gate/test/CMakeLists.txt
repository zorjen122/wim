cmake_minimum_required(VERSION 3.16)
project(test_service LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(test_service "test_service.cc")


target_include_directories(test_service PUBLIC
    /usr/include/boost
    /usr/include/jsoncpp
)

target_link_libraries(test_service jsoncpp boost_system pthread)
