cmake_minimum_required(VERSION 3.14)

project(perfxagent-app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置版本号
set(PERFXAGENT_VERSION_MAJOR 1)
set(PERFXAGENT_VERSION_MINOR 0)
set(PERFXAGENT_VERSION_PATCH 0)
set(PERFXAGENT_VERSION "${PERFXAGENT_VERSION_MAJOR}.${PERFXAGENT_VERSION_MINOR}.${PERFXAGENT_VERSION_PATCH}")

# 添加编译警告选项
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 平台特定配置
if(APPLE)
    # macOS specific configuration
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew/opt/qt@6")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew/opt/openssl@3")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew/opt/portaudio")
    set(Qt6_DIR "/opt/homebrew/opt/qt@6/lib/cmake/Qt6")
    set(BOOST_ROOT "/opt/homebrew/opt/boost")
    set(BOOST_INCLUDEDIR "/opt/homebrew/opt/boost/include")
    set(BOOST_LIBRARYDIR "/opt/homebrew/opt/boost/lib")
endif()

if(WIN32)
    # Windows specific configuration
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    add_compile_options(/W4 /MP)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
elseif(ANDROID)
    # Android specific configuration
    set(CMAKE_ANDROID_NDK ${ANDROID_NDK})
    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})
    set(CMAKE_ANDROID_STL_TYPE c++_shared)
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖包
find_package(Qt6 6.4.0 COMPONENTS 
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
    WebSockets
    Multimedia
    REQUIRED
)

find_package(PortAudio 19.7.0 REQUIRED)

# Boost 配置
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# 查找 Boost 组件
find_package(Boost 1.74.0 REQUIRED COMPONENTS system thread)

# 设置 Boost 包含目录
if(APPLE)
    set(BOOST_ROOT "/opt/homebrew/opt/boost")
    set(BOOST_INCLUDEDIR "/opt/homebrew/opt/boost/include")
    set(BOOST_LIBRARYDIR "/opt/homebrew/opt/boost/lib")
endif()

# 添加 Boost.Beast 头文件路径
include_directories(${BOOST_INCLUDEDIR})
include_directories(/opt/homebrew/include)
include_directories(/opt/homebrew/opt/boost/include)

find_package(OpenSSL 3.0.0 REQUIRED)

# 添加nlohmann/json
find_package(nlohmann_json 3.11.3 REQUIRED)

# 添加源文件
set(SOURCES
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/agent_zoo.cpp
    src/ui/device_settings.cpp
    src/ui/hello_window.cpp
    src/audio/audio_device.cpp
    src/audio/audio_config.cpp
    src/audio/audio_thread.cpp
    src/core/config_manager.cpp
    src/connection/websocket_client.cpp
    src/connection/websocket_message.cpp
    src/connection/thread_pool.cpp
    src/connection/connection_manager.cpp
    src/audio/audio_manager.cpp
)

# 添加头文件
set(HEADERS
    include/ui/main_window.h
    include/ui/agent_zoo.h
    include/ui/device_settings.h
    include/ui/hello_window.h
    include/audio/audio_device.h
    include/audio/audio_thread.h
    include/audio/audio_device_info.h
    include/audio/audio_config.h
    include/core/config_manager.h
    include/connection/websocket_client.h
    include/connection/websocket_message.h
    include/connection/thread_pool.h
    include/connection/connection_manager.h
    include/audio/audio_manager.h
)

# 创建可执行文件
add_executable(perfxagent-app ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(perfxagent-app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::WebSockets
    Qt6::Multimedia
    ${PortAudio_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::thread
    /opt/homebrew/lib/libboost_url.dylib
)

# 复制 Qt DLL 到输出目录（仅 Windows）
if(WIN32)
    add_custom_command(TARGET perfxagent-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE:Qt6::Network>
            $<TARGET_FILE:Qt6::WebEngineWidgets>
            $<TARGET_FILE:Qt6::WebSockets>
            $<TARGET_FILE_DIR:perfxagent-app>
    )
endif()

install(TARGETS perfxagent-app
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 设置包含目录
target_include_directories(perfxagent-app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebEngineWidgets_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${BOOST_INCLUDEDIR}
    /opt/homebrew/include
    /opt/homebrew/opt/boost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/connection
    ${nlohmann_json_INCLUDE_DIRS}
)

# 设置输出目录
set_target_properties(perfxagent-app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 音频库源文件
set(AUDIO_SOURCES
    src/audio/audio_manager.cpp
    src/audio/audio_device.cpp
    src/audio/audio_thread.cpp
)

# 音频库头文件
set(AUDIO_HEADERS
    include/audio/audio_manager.h
    include/audio/audio_device.h
    include/audio/audio_thread.h
    include/audio/audio_device_info.h
)

# 创建音频库
add_library(perfx_audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS})

# 设置音频库的包含目录
target_include_directories(perfx_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
)

# 链接音频库的依赖
target_link_libraries(perfx_audio PUBLIC
    Qt6::Core
    ${PortAudio_LIBRARIES}
)

# 添加示例程序
add_executable(audio_example examples/audio_example.cpp)

# 设置示例程序的包含目录
target_include_directories(audio_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接示例程序的依赖
target_link_libraries(audio_example PRIVATE
    perfx_audio
)

# 安装规则
install(TARGETS perfx_audio audio_example
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
) 