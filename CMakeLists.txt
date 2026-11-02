cmake_minimum_required(VERSION 3.24)

project(EdaCourse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(validate_design
    tools/validate_design.cpp
    tools/json_lite.hpp
)

target_include_directories(validate_design PRIVATE tools)

if(MSVC)
    target_compile_options(validate_design PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(validate_design PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT validate_design)

enable_testing()
add_test(
    NAME validate_design_samples
    COMMAND validate_design "${CMAKE_CURRENT_SOURCE_DIR}"
)
