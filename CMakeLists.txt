cmake_minimum_required(VERSION 3.24)

project(EdaCourse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ==================== 网表 / 设计校验器 ====================
add_executable(validate_design
    tools/validate_design.cpp
    tools/json_lite.hpp
)
target_include_directories(validate_design PRIVATE tools)

# ==================== PCB 核心数据模型与持久化 ====================
add_library(pcbcore STATIC
    src/io/Json.cpp
    src/io/ProjectIO.cpp
)
target_include_directories(pcbcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(pcbtests tests/test_main.cpp)
target_link_libraries(pcbtests PRIVATE pcbcore)

add_executable(pcbdemo app/main.cpp)
target_link_libraries(pcbdemo PRIVATE pcbcore)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT pcbdemo)

# ==================== 测试 ====================
enable_testing()
add_test(NAME validate_design_samples COMMAND validate_design "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME pcbtests COMMAND pcbtests)
