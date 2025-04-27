cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(state-test)

file(GLOB TARGET_CC cmd/*.cc)
file(GLOB LINK_CC)  # 待链接的源文件


foreach(SRC_FILE ${TARGET_CC})
    # 获取文件名，不带路径GLOB LINK_CC *.cc和扩展名
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    # 创建一个新的可执行文件，每个 .cc 文件对应一个可执行文件
    add_executable(${EXEC_NAME} ${SRC_FILE} ${LINK_CC})

   # 设置每个可执行文件的输出目录为 ./bin
    set_target_properties(${EXEC_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ./stateBin
    )

    # 设置每个可执行文件的包含目录
    target_include_directories(${EXEC_NAME} PRIVATE
      ../../public/include/
      include/
    )

    target_link_directories(${EXEC_NAME} PRIVATE ../../public/build/)

    # 设置每个可执行文件的链接库
    target_link_libraries(${EXEC_NAME} PRIVATE
      imPublic
      mysqlcppconn8 
      spdlog 
      yaml-cpp
      hiredis
      redis++
    )

    # 添加编译选项
    target_compile_options(${EXEC_NAME} PRIVATE --std=c++17 -Wall -g)
endforeach()
