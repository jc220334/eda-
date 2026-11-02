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
    src/pcb/Geometry.cpp
    src/pcb/LayoutService.cpp
    src/pcb/NetlistImporter.cpp
    src/pcb/PcbWorkflow.cpp
    src/pcb/RoutingService.cpp
)
target_include_directories(pcbcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(pcbtests tests/test_main.cpp)
target_link_libraries(pcbtests PRIVATE pcbcore)

add_executable(pcbmembertests tests/member_c_tests.cpp)
target_link_libraries(pcbmembertests PRIVATE pcbcore)

add_executable(pcbdemo app/main.cpp)
target_link_libraries(pcbdemo PRIVATE pcbcore)

# ==================== wxWidgets 图形界面 ====================
option(EDA_BUILD_GUI "Build the interactive wxWidgets PCB editor" ON)

if(EDA_BUILD_GUI)
    set(wxWidgets_ROOT_DIR "D:/wxwidgets" CACHE PATH "wxWidgets root directory")
    set(wxWidgets_LIB_DIR "${wxWidgets_ROOT_DIR}/lib/vc_x64_lib" CACHE PATH "wxWidgets library directory")
    set(wxWidgets_CONFIGURATION mswu CACHE STRING "wxWidgets library configuration")
    set(wxWidgets_USE_STATIC ON)
    set(wxWidgets_USE_REL_AND_DBG ON)
    find_package(wxWidgets REQUIRED COMPONENTS core base adv)
    include(${wxWidgets_USE_FILE})

    add_executable(eda_gui WIN32
        app/gui_main.cpp
        src/ui/BoardCanvas.cpp
        src/ui/BoardCanvas.h
        src/ui/MainFrame.cpp
        src/ui/MainFrame.h
    )
    target_link_libraries(eda_gui PRIVATE pcbcore ${wxWidgets_LIBRARIES})
    target_include_directories(eda_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${wxWidgets_INCLUDE_DIRS})

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT eda_gui)
else()
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT pcbdemo)
endif()

# ==================== 测试 ====================
enable_testing()
add_test(NAME validate_design_samples COMMAND validate_design "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME pcbtests COMMAND pcbtests)
add_test(NAME pcbmembertests COMMAND pcbmembertests "${CMAKE_CURRENT_SOURCE_DIR}")
