cmake_minimum_required(VERSION 3.10)
project(file_service_test)

# 查找依赖
find_package(gRPC REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# 设置测试源文件
file(GLOB unit_test_sources unit/*.cc)
file(GLOB integration_test_sources integration/*.cc)

enable_testing()

# 单元测试
foreach(test_source ${unit_test_sources})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})

    target_link_directories(${test_name} PRIVATE 
    ${CMAKE_SOURCE_DIR}/../build/lib
    ${CMAKE_SOURCE_DIR}/../../public/build
    )

    target_link_libraries(${test_name}
        fileService
        GTest::GTest 
        GTest::Main
        ${CMAKE_THREAD_LIBS_INIT}
        grpc++_test_util  # 仅单元测试需要mock支持
    )

    target_include_directories(${test_name} PRIVATE 
      ${CMAKE_SOURCE_DIR}/../include 
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/../../public/include
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endforeach()

# 集成测试
foreach(test_source ${integration_test_sources})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})

    target_link_directories(${test_name} PRIVATE 
    ${CMAKE_SOURCE_DIR}/../build/lib
    ${CMAKE_SOURCE_DIR}/../../public/build
    )

    target_link_libraries(${test_name}
        fileService
        GTest::GTest 
        GTest::Main
        grpc++
        grpc++_reflection
        ${CMAKE_THREAD_LIBS_INIT}
    )
        
    target_include_directories(${test_name} PRIVATE 
      ${CMAKE_SOURCE_DIR}/../include 
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/../../public/include
    )

    
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endforeach()