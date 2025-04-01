# 该CMakeLists.txt文件用于为calngd生成编译命令缓存文件，以便进行代码补全、跳转等功能。
cmake_minimum_required(VERSION 3.10)
project(publicLib)
# 启用 clangd 缓存生成
set(CMAKE_EXPORT_COMPILE_COMMANDS true)


find_package(Threads REQUIRED)
message(STATUS "Using Threads ${CMAKE_THREAD_LIBS_INIT}")

find_package(Boost REQUIRED COMPONENTS system filesystem)  
message(STATUS "Using Boost ${Boost_VERSION}")

# protobuf link
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# 第三方库头文件
set(_DIR_JSONCPP /usr/include/jsoncpp)
set(_DIR_HIREDIS /usr/include/hiredis)
set(_DIR_MYSQLCPPCONN /usr/include/)
set(_DIR_YAML_CPP_DIR /usr/local/include/)
set(_DIR_SPDLOG /usr/local/spdlog/include)
set(_DIR_LOCAL_INCLUDE ./include)

# 添加 src 目录下的所有源文件
file(GLOB SOURCES "src/*.cc")

# 添加一个可执行文件（这里假设你需要一个可执行文件来生成 compile_commands.json，
# 即便这个项目本身不会生成可执行文件，clangd 需要它来获取编译信息）
add_executable(publicLib ${SOURCES})

target_include_directories(publicLib PUBLIC
    ${_DIR_JSONCPP}
    ${_DIR_HIREDIS}
    ${_DIR_MYSQLCPPCONN}
    ${_DIR_SPDLOG}
    ${_DIR_YAML_CPP_DIR}
    ${_DIR_LOCAL_INCLUDE}
)
