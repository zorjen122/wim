# bug，暂时无法使用
cmake_minimum_required(VERSION 3.15)
project(WimSystem VERSION 1.0 LANGUAGES CXX)

# 全局配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 为工具链提供编译数据库

# 公共配置
option(BUILD_TESTING "Build tests" ON)
option(BUILD_DEMOS "Build demo programs" OFF)

# 公共库路径
set(PUBLIC_DIR ${CMAKE_SOURCE_DIR}/public)

# 包含子项目
add_subdirectory(public)
add_subdirectory(chat)
add_subdirectory(gate)
add_subdirectory(state)

if(BUILD_TESTING)
    add_subdirectory(test) # 业务集成测试
endif()
if(BUILD_DEMOS)
    add_subdirectory(demo) # 示例代码
endif()