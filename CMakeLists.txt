cmake_minimum_required(VERSION 3.16)
project(PcbCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 核心库：数据模型（header-only）+ JSON + 文件读写
add_library(pcbcore STATIC
    src/io/Json.cpp
    src/io/ProjectIO.cpp
)
target_include_directories(pcbcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 测试程序
add_executable(pcbtests tests/test_main.cpp)
target_link_libraries(pcbtests PRIVATE pcbcore)

# 集成演示程序
add_executable(pcbdemo app/main.cpp)
target_link_libraries(pcbdemo PRIVATE pcbcore)
