cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(mysql-test)

file(GLOB PUBLIC_CC ${CMAKE_SOURCE_DIR}/../../src/Configer.cc)
file(GLOB SOURCE_CC mysqlDaoTest.cc redisDaoTest.cc)


set(_PUBLIC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../../include)



foreach(SRC_FILE ${SOURCE_CC})
    # 获取文件名，不带路径和扩展名
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    # 创建一个新的可执行文件，每个 .cc 文件对应一个可执行文件
    add_executable(${EXEC_NAME} ${SRC_FILE} ${PUBLIC_CC})

   # 设置每个可执行文件的输出目录为 ./bin
    set_target_properties(${EXEC_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # 设置每个可执行文件的包含目录
    target_include_directories(${EXEC_NAME} PUBLIC
      /usr/local/spdlog/include
      ${_PUBLIC_INCLUDE_DIR}
    )

    target_link_directories(${EXEC_NAME} PUBLIC 
      /usr/local/spdlog/lib 
      /usr/local/yaml-cpp/lib
    )

    # 设置每个可执行文件的链接库
    target_link_libraries(${EXEC_NAME} PUBLIC
      mysqlcppconn8 
      spdlog 
      yaml-cpp
      hiredis
      redis++
    )

    # 添加编译选项
    target_compile_options(${EXEC_NAME} PRIVATE --std=c++17 -Wall -g)
endforeach()
