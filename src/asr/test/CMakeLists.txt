cmake_minimum_required(VERSION 3.10)
project(websocket_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_DEFAULT_MAJOR_VERSION 6)

# 设置 Qt 查找路径（macOS）
if(APPLE)
    execute_process(
        COMMAND brew --prefix qt@6
        OUTPUT_VARIABLE QT6_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE QT6_RESULT
    )
    if(NOT QT6_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find Qt6 prefix. Please ensure Qt6 is installed via Homebrew.")
    endif()
    
    set(CMAKE_PREFIX_PATH 
        ${CMAKE_PREFIX_PATH} 
        "${QT6_PREFIX}"
    )
    set(Qt6_DIR "${QT6_PREFIX}/lib/cmake/Qt6")
endif()

# 查找 Qt6 组件
find_package(Qt6 COMPONENTS Core Network WebSockets REQUIRED)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please install Qt6.")
endif()

message(STATUS "Qt6 found: ${Qt6_VERSION}")

# 创建测试程序
add_executable(test_qt_websocket test_qt_websocket.cpp)

# 设置包含目录
target_include_directories(test_qt_websocket PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
)

# 设置 MOC 特定选项
set_target_properties(test_qt_websocket PROPERTIES
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include"
)

# 链接 Qt6 库
target_link_libraries(test_qt_websocket 
    Qt6::Core 
    Qt6::Network 
    Qt6::WebSockets
)

# 设置输出目录
set_target_properties(test_qt_websocket PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 添加编译选项
if(MSVC)
    target_compile_options(test_qt_websocket PRIVATE /W4)
else()
    target_compile_options(test_qt_websocket PRIVATE -Wall -Wextra)
endif()

# 添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_qt_websocket PRIVATE -g -O0)
    target_compile_definitions(test_qt_websocket PRIVATE DEBUG)
else()
    target_compile_options(test_qt_websocket PRIVATE -O3)
    target_compile_definitions(test_qt_websocket PRIVATE NDEBUG)
endif()

# 显示构建信息
message(STATUS "Building test_qt_websocket executable")
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin") 