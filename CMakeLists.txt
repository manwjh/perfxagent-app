cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)
cmake_policy(SET CMP0079 NEW)

cmake_minimum_required(VERSION 3.10)

project(perfxagent-app)

# 设置 C++ 标准
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
set(PERFXAGENT_VERSION_MINOR 6)
set(PERFXAGENT_VERSION_PATCH 1)
set(PERFXAGENT_VERSION "${PERFXAGENT_VERSION_MAJOR}.${PERFXAGENT_VERSION_MINOR}.${PERFXAGENT_VERSION_PATCH}")

# 添加编译警告选项
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# 设置 Qt 相关配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 确保使用 Qt6
set(QT_DEFAULT_MAJOR_VERSION 6)

# 设置 Qt 查找路径
set(CMAKE_PREFIX_PATH 
    ${CMAKE_PREFIX_PATH}
    /opt/homebrew/opt/qt@6
    /opt/homebrew/opt/qt6
    /usr/local/opt/qt@6
    /usr/local/opt/qt6
)

# 查找 Qt6 组件
message(STATUS "Finding Qt6...")
find_package(Qt6 COMPONENTS 
    Core 
    Widgets 
    Network 
    Gui 
    Multimedia 
    WebEngineWidgets 
    WebSockets
    Concurrent    # 添加并发支持
    PrintSupport  # 添加打印支持
    REQUIRED
)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please install Qt6.")
endif()
message(STATUS "Qt6 found: ${Qt6_VERSION}")

# 设置 Qt6 包含目录
if(Qt6_FOUND)
    include_directories(
        ${Qt6Core_INCLUDE_DIRS}
        ${Qt6Widgets_INCLUDE_DIRS}
        ${Qt6Network_INCLUDE_DIRS}
        ${Qt6Gui_INCLUDE_DIRS}
        ${Qt6Multimedia_INCLUDE_DIRS}
        ${Qt6WebEngineWidgets_INCLUDE_DIRS}
        ${Qt6WebSockets_INCLUDE_DIRS}
        ${Qt6Concurrent_INCLUDE_DIRS}
        ${Qt6PrintSupport_INCLUDE_DIRS}
    )
    
    # 添加 Qt6 定义
    add_definitions(${Qt6Core_DEFINITIONS})
    add_definitions(${Qt6Widgets_DEFINITIONS})
    
    # 设置 Qt6 编译选项
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Core_EXECUTABLE_COMPILE_FLAGS}")
endif()

# 平台特定配置
if(APPLE)
    # macOS specific configuration
    execute_process(
        COMMAND brew --prefix qt@6
        OUTPUT_VARIABLE QT6_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE QT6_RESULT
    )
    if(NOT QT6_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find Qt6 prefix. Please ensure Qt6 is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE OPENSSL_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE OPENSSL_RESULT
    )
    if(NOT OPENSSL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find OpenSSL prefix. Please ensure OpenSSL is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix portaudio
        OUTPUT_VARIABLE PORTAUDIO_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PORTAUDIO_RESULT
    )
    if(NOT PORTAUDIO_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find PortAudio prefix. Please ensure PortAudio is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix boost
        OUTPUT_VARIABLE BOOST_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE BOOST_RESULT
    )
    if(NOT BOOST_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find Boost prefix. Please ensure Boost is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix opus
        OUTPUT_VARIABLE OPUS_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE OPUS_RESULT
    )
    if(NOT OPUS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find Opus prefix. Please ensure Opus is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix libogg
        OUTPUT_VARIABLE OGG_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE OGG_RESULT
    )
    if(NOT OGG_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find OGG prefix. Please ensure OGG is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix nlohmann-json
        OUTPUT_VARIABLE NLOHMANN_JSON_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NLOHMANN_JSON_RESULT
    )
    if(NOT NLOHMANN_JSON_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find nlohmann-json prefix. Please ensure nlohmann-json is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix websocketpp
        OUTPUT_VARIABLE WEBSOCKETPP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE WEBSOCKETPP_RESULT
    )
    if(NOT WEBSOCKETPP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find websocketpp prefix. Please ensure websocketpp is installed via Homebrew.")
    endif()

    execute_process(
        COMMAND brew --prefix asio
        OUTPUT_VARIABLE ASIO_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE ASIO_RESULT
    )
    if(NOT ASIO_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to find asio prefix. Please ensure asio is installed via Homebrew.")
    endif()
    
    set(CMAKE_PREFIX_PATH 
        ${CMAKE_PREFIX_PATH} 
        "${QT6_PREFIX}"
        "${OPENSSL_PREFIX}"
        "${PORTAUDIO_PREFIX}"
        "${OPUS_PREFIX}"
        "${OGG_PREFIX}"
        "${NLOHMANN_JSON_PREFIX}"
        "${WEBSOCKETPP_PREFIX}"
        "${ASIO_PREFIX}"
    )
    set(Qt6_DIR "${QT6_PREFIX}/lib/cmake/Qt6")
    set(BOOST_ROOT "${BOOST_PREFIX}")
    set(BOOST_INCLUDEDIR "${BOOST_PREFIX}/include")
    set(BOOST_LIBRARYDIR "${BOOST_PREFIX}/lib")
    include_directories(${PORTAUDIO_PREFIX}/include)
    include_directories(${NLOHMANN_JSON_PREFIX}/include)
    include_directories(${WEBSOCKETPP_PREFIX}/include)
    include_directories(${ASIO_PREFIX}/include)
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

# 查找必要的包
message(STATUS "Finding OpenCV...")
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")

message(STATUS "Finding PkgConfig...")
find_package(PkgConfig REQUIRED)
message(STATUS "PkgConfig found: ${PKG_CONFIG_VERSION}")

message(STATUS "Finding PortAudio...")
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
message(STATUS "PortAudio found: ${PORTAUDIO_VERSION}")

message(STATUS "Finding Opus...")
pkg_check_modules(OPUS REQUIRED opus)
message(STATUS "Opus found: ${OPUS_VERSION}")

message(STATUS "Finding OGG...")
pkg_check_modules(OGG REQUIRED ogg)
message(STATUS "OGG found: ${OGG_VERSION}")

message(STATUS "Finding SndFile...")
pkg_check_modules(SNDFILE REQUIRED sndfile)
message(STATUS "SndFile found: ${SNDFILE_VERSION}")

# 添加 nlohmann_json
message(STATUS "Finding nlohmann_json...")
find_package(nlohmann_json 3.11.3 REQUIRED)
message(STATUS "nlohmann_json found: ${nlohmann_json_VERSION}")

# 添加头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
    ${ASIO_PREFIX}/include
)

# 添加库目录
link_directories(
    ${OpenCV_LIBRARY_DIRS}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${OPUS_LIBRARY_DIRS}
    ${OGG_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# 查找 Boost 和 OpenSSL
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# 添加子目录
add_subdirectory(src)
add_subdirectory(examples)

# 构建 ixwebsocket 库
add_subdirectory(third_party/ixwebsocket)

# macOS 特定配置
if(APPLE)
    set(MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.perfx.agent")
    set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PERFXAGENT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PERFXAGENT_VERSION}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PERFXAGENT_VERSION}")
    set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} Version ${PERFXAGENT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 PerfX. All rights reserved.")
endif()

# 安装目标
if(APPLE)
    install(TARGETS perfxagent-app
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS perfxagent-app
        RUNTIME DESTINATION bin
    )
endif()

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

# 确保正确链接 PortAudio 库
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L/opt/homebrew/lib")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L/opt/homebrew/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L/opt/homebrew/lib")
endif()

# 确保正确链接 Opus 库
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib")
endif() 