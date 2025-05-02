cmake_minimum_required(VERSION 3.1)
project(file)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
set(protobuf_MODULE_COMPATIBLE TRUE)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 源文件分组
file(GLOB SOURCE "src/*.cc")
file(GLOB PROTO_SOURCES "rpc/*.cc")

# 创建库
add_library(fileService STATIC ${SOURCE} ${PROTO_SOURCES})

# 创建可执行文件
add_executable(file ${SOURCE} ${PROTO_SOURCES})

# 公共包含路径
set(COMMON_INCLUDES
    ./include
    ./../public/include
    ./rpc
)


# 设置包含路径
target_include_directories(fileService PUBLIC ${COMMON_INCLUDES})
target_include_directories(file PUBLIC ${COMMON_INCLUDES})

# 链接设置
target_link_directories(fileService PUBLIC ../public/build)
target_link_directories(file PUBLIC ../public/build)
target_link_libraries(fileService PUBLIC
    imPublic
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    spdlog
    yaml-cpp
    Threads::Threads
)

target_link_libraries(file
    fileService
    imPublic
)

# 编译选项
target_compile_options(fileService PRIVATE --std=c++17 -Wall -g)
target_compile_options(file PRIVATE --std=c++17 -Wall -g)

# 测试
if(BUILD_TESTING)
    add_subdirectory(test)
endif()