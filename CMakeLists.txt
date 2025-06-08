cmake_minimum_required(VERSION 3.16)

project(perfxagent-app VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

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
    execute_process(
        COMMAND brew --prefix qt@6
        OUTPUT_VARIABLE QT6_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE OPENSSL_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix portaudio
        OUTPUT_VARIABLE PORTAUDIO_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix boost
        OUTPUT_VARIABLE BOOST_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix opus
        OUTPUT_VARIABLE OPUS_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix libogg
        OUTPUT_VARIABLE OGG_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix nlohmann-json
        OUTPUT_VARIABLE NLOHMANN_JSON_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    set(CMAKE_PREFIX_PATH 
        ${CMAKE_PREFIX_PATH} 
        "${QT6_PREFIX}"
        "${OPENSSL_PREFIX}"
        "${PORTAUDIO_PREFIX}"
        "${OPUS_PREFIX}"
        "${OGG_PREFIX}"
        "${NLOHMANN_JSON_PREFIX}"
    )
    set(Qt6_DIR "${QT6_PREFIX}/lib/cmake/Qt6")
    set(BOOST_ROOT "${BOOST_PREFIX}")
    set(BOOST_INCLUDEDIR "${BOOST_PREFIX}/include")
    set(BOOST_LIBRARYDIR "${BOOST_PREFIX}/lib")
    include_directories(${PORTAUDIO_PREFIX}/include)
    include_directories(/opt/homebrew/opt/portaudio/include)
    include_directories(${NLOHMANN_JSON_PREFIX}/include)
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
find_package(Opus REQUIRED)
find_package(Ogg REQUIRED)
find_package(SndFile REQUIRED)
find_package(SAMPLERATE REQUIRED)
find_package(nlohmann_json 3.11.3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)

# RNNoise
set(RNNOISE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise)
set(RNNOISE_SOURCES
    ${RNNOISE_ROOT}/src/denoise.c
    ${RNNOISE_ROOT}/src/rnnoise_data.c
    ${RNNOISE_ROOT}/src/kiss_fft.c
    ${RNNOISE_ROOT}/src/pitch.c
    ${RNNOISE_ROOT}/src/rnn.c
    ${RNNOISE_ROOT}/src/nnet.c
    ${RNNOISE_ROOT}/src/nnet_default.c
    ${RNNOISE_ROOT}/src/celt_lpc.c
    ${RNNOISE_ROOT}/src/rnnoise_tables.c
    ${RNNOISE_ROOT}/src/parse_lpcnet_weights.c
)
include_directories(
    ${RNNOISE_ROOT}/include
    ${RNNOISE_ROOT}/src
)
add_definitions(-DHAVE_CONFIG_H)

# RNNoise compilation options
add_definitions(-DHAVE_CONFIG_H)

# Include RNNoise headers and source files
include_directories(
    ${RNNOISE_ROOT}/src
)

# Set compilation flags for Apple platforms
if(APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
endif()

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

# GTest 手动路径设置（Homebrew 安装）
set(GTEST_ROOT /opt/homebrew/Cellar/googletest/1.17.0)
set(GTEST_INCLUDE_DIR ${GTEST_ROOT}/include)
set(GTEST_LIBRARY ${GTEST_ROOT}/lib/libgtest.a)
set(GTEST_MAIN_LIBRARY ${GTEST_ROOT}/lib/libgtest_main.a)
include_directories(${GTEST_INCLUDE_DIR})
# 只链接一次 GTest 库
link_libraries(${GTEST_LIBRARY})

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

# 添加源文件
set(SOURCES
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/agent_zoo.cpp
    src/ui/device_settings.cpp
    src/ui/hello_window.cpp
    src/audio/audio_device.cpp
    src/connection/thread_pool.cpp
    src/connection/connection_manager.cpp
    src/audio/audio_manager.cpp
    src/camera/camera_device.cpp
    src/camera/camera_manager.cpp
    ${RNNOISE_SOURCES}
)

# 添加头文件
set(HEADERS
    include/ui/main_window.h
    include/ui/agent_zoo.h
    include/ui/device_settings.h
    include/ui/hello_window.h
    include/audio/audio_device.h
    include/connection/thread_pool.h
    include/connection/connection_manager.h
    include/audio/audio_manager.h
    include/camera/camera_device.h
    include/camera/camera_manager.h
    include/camera/camera_types.h
)

# 添加主项目包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/connection
    ${CMAKE_CURRENT_SOURCE_DIR}/include/camera
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebEngineWidgets_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${QT6_PREFIX}/include
    ${QT6_PREFIX}/include/QtCore
    ${QT6_PREFIX}/include/QtGui
    ${QT6_PREFIX}/include/QtWidgets
    ${QT6_PREFIX}/include/QtNetwork
    ${QT6_PREFIX}/include/QtWebEngineWidgets
    ${QT6_PREFIX}/include/QtWebSockets
    ${QT6_PREFIX}/include/QtMultimedia
    ${OpenCV_INCLUDE_DIRS}
)

# 添加音频库源文件
set(AUDIO_SOURCES
    src/audio/audio_device.cpp
    src/audio/audio_processor.cpp
    src/audio/audio_thread.cpp
    src/audio/audio_manager.cpp
)

# 添加音频库头文件
set(AUDIO_HEADERS
    include/audio/audio_types.h
    include/audio/audio_device.h
    include/audio/audio_processor.h
    include/audio/audio_thread.h
    include/audio/audio_manager.h
)

# 创建音频库
add_library(perfx_audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS} ${RNNOISE_SOURCES})

# 设置音频库包含目录
target_include_directories(perfx_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
    ${PORTAUDIO_PREFIX}/include
    ${SndFile_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
    ${RNNOISE_ROOT}/include
    ${RNNOISE_ROOT}/src
)

# 链接音频库依赖
target_link_libraries(perfx_audio
    ${PortAudio_LIBRARIES}
    ${Opus_LIBRARIES}
    ${Ogg_LIBRARIES}
    ${SndFile_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
    PkgConfig::FFMPEG
)

# 设置编译选项
target_compile_features(perfx_audio PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(perfx_audio PRIVATE /W4 /WX)
else()
    target_compile_options(perfx_audio PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 添加连接库源文件
set(CONNECTION_SOURCES
    src/connection/thread_pool.cpp
    src/connection/connection_manager.cpp
)

# 添加连接库头文件
set(CONNECTION_HEADERS
    include/connection/thread_pool.h
    include/connection/connection_manager.h
)

# 创建连接库
add_library(perfx_connection STATIC ${CONNECTION_SOURCES} ${CONNECTION_HEADERS})

# 设置连接库包含目录
target_include_directories(perfx_connection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# 链接连接库依赖
target_link_libraries(perfx_connection PUBLIC
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# 设置编译选项
target_compile_features(perfx_connection PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(perfx_connection PRIVATE /W4 /WX)
else()
    target_compile_options(perfx_connection PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 创建可执行文件
add_executable(perfxagent-app ${SOURCES} ${HEADERS} ${RNNOISE_SOURCES})

# 添加examples目录
add_subdirectory(examples)

# 设置可执行文件的包含目录
target_include_directories(perfxagent-app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
    ${Opus_INCLUDE_DIRS}
    ${Ogg_INCLUDE_DIRS}
    ${SndFile_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)

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
    ${Opus_LIBRARIES}
    ${Ogg_LIBRARIES}
    ${SndFile_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::thread
    perfx_audio
    perfx_connection
    ${OpenCV_LIBS}
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
    ${Opus_INCLUDE_DIRS}
    ${Ogg_INCLUDE_DIRS}
    ${SndFile_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebEngineWidgets_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${BOOST_INCLUDEDIR}
    /opt/homebrew/include
    /opt/homebrew/opt/boost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/connection
    ${CMAKE_CURRENT_SOURCE_DIR}/include/camera
)

# 设置输出目录
set_target_properties(perfxagent-app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS perfx_audio
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/audio)

# 添加测试
enable_testing()
# add_subdirectory(tests/audio)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${PortAudio_INCLUDE_DIRS})
include_directories(${Opus_INCLUDE_DIRS})
include_directories(${SAMPLERATE_INCLUDE_DIRS})
include_directories(${nlohmann_json_INCLUDE_DIRS}) 