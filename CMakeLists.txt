cmake_minimum_required(VERSION 3.16)

project(perfxagent-app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add Homebrew paths for macOS
if(APPLE)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew")
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Windows 特定配置
if(WIN32)
    # 设置 Windows 子系统
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    
    # 添加 Windows 特定的编译选项
    add_compile_options(/W4 /MP)
    
    # 设置 DLL 输出目录
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)
find_package(PortAudio REQUIRED)

# 设置 Qt 头文件路径
set(QT_INCLUDE_DIRS
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
    ${QT_INCLUDE_DIRS}
)

# 收集源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS 
    "include/*.h"
)

# 创建可执行文件
add_executable(perfxagent-app WIN32 ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(perfxagent-app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    ${PortAudio_LIBRARIES}
)

# 复制 Qt DLL 到输出目录（仅 Windows）
if(WIN32)
    add_custom_command(TARGET perfxagent-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE:Qt6::Network>
            $<TARGET_FILE:Qt6::WebSockets>
            $<TARGET_FILE_DIR:perfxagent-app>
    )
endif()

install(TARGETS perfxagent-app
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
) 