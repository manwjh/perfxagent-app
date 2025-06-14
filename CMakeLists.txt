cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)

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
set(PERFXAGENT_VERSION_MINOR 1)
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

# FFMPEG 配置
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
    src/audio/audio_device.cpp
    src/audio/audio_manager.cpp
    src/camera/camera_device.cpp
    src/camera/camera_manager.cpp
)

# 添加头文件
set(HEADERS
    include/ui/main_window.h
    include/audio/audio_device.h
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

# 添加音频库
add_library(perfx_audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS})
target_link_libraries(perfx_audio
    PRIVATE
    ${PortAudio_LIBRARIES}
    ${Opus_LIBRARIES}
    ${SndFile_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
)

# 确保 FFmpeg 库路径正确
if(APPLE)
    set(FFMPEG_LIBRARY_DIR "/opt/homebrew/lib")
    link_directories(${FFMPEG_LIBRARY_DIR})
endif()

# 确保 Ogg 库路径正确
if(APPLE)
    set(OGG_LIBRARY_DIR "/opt/homebrew/lib")
    link_directories(${OGG_LIBRARY_DIR})
endif()

# 添加主可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    perfx_audio
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::Multimedia
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Boost::thread
    ${OpenCV_LIBS}
    ${GTEST_LIBRARY}
    ${FFMPEG_LIBRARIES}
    ogg  # 确保 Ogg 库正确链接
)

# 添加 examples 子目录
add_subdirectory(examples)

# 安装目标
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装资源文件
install(DIRECTORY resources/
    DESTINATION share/${PROJECT_NAME}/resources
    FILES_MATCHING PATTERN "*.png"
    PATTERN "*.jpg"
    PATTERN "*.svg"
    PATTERN "*.qss"
)

# 安装配置文件
install(FILES config.json
    DESTINATION share/${PROJECT_NAME}
)

# 安装文档
install(FILES README.md LICENSE
    DESTINATION share/${PROJECT_NAME}/doc
) 