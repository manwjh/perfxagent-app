cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)
cmake_policy(SET CMP0079 NEW)

cmake_minimum_required(VERSION 3.10)

# 设置版本号 - 必须在project()之前设置
set(PERFXAGENT_VERSION_MAJOR 1)
set(PERFXAGENT_VERSION_MINOR 6)
set(PERFXAGENT_VERSION_PATCH 2)
set(PERFXAGENT_VERSION "${PERFXAGENT_VERSION_MAJOR}.${PERFXAGENT_VERSION_MINOR}.${PERFXAGENT_VERSION_PATCH}")

project(perfxagent-app VERSION ${PERFXAGENT_VERSION})

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

# 添加编译警告选项
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

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
    Concurrent
    PrintSupport
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
    
    add_definitions(${Qt6Core_DEFINITIONS})
    add_definitions(${Qt6Widgets_DEFINITIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Core_EXECUTABLE_COMPILE_FLAGS}")
endif()

# macOS 依赖配置
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

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找必要的包
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
    ${PORTAUDIO_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
    ${ASIO_PREFIX}/include
)

# 添加库目录
link_directories(
    ${PORTAUDIO_LIBRARY_DIRS}
    ${OPUS_LIBRARY_DIRS}
    ${OGG_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# 查找 Boost 和 OpenSSL
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# =============================================================================
# CPack 配置 - macOS 安装包生成
# =============================================================================

# 启用CPack
include(CPack)

# 设置CPack基本信息
set(CPACK_PACKAGE_NAME "PerfxAgent-ASR")
set(CPACK_PACKAGE_VENDOR "PerfX")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "音频处理与实时语音识别应用")
set(CPACK_PACKAGE_VERSION "${PERFXAGENT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PERFXAGENT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PERFXAGENT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PERFXAGENT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PerfxAgent-ASR")

# macOS 特定配置
set(CPACK_GENERATOR "DragNDrop;TGZ")
set(CPACK_DMG_VOLUME_NAME "PerfxAgent-ASR ${PERFXAGENT_VERSION}")
set(CPACK_DMG_FORMAT "UDZO")
set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/setup_dmg_ds_store.sh")
set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/cmake/dmg_background.png")

# 应用程序图标和元数据
set(CPACK_BUNDLE_NAME "PerfxAgent-ASR")
set(CPACK_BUNDLE_PLIST "${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")
set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/icons/app_icon.icns")

# 设置应用图标路径
set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/app_icon.icns")
set(APP_ICON_WINDOWS "${CMAKE_SOURCE_DIR}/resources/icons/app_icon.ico")
set(APP_ICON_PNG "${CMAKE_SOURCE_DIR}/resources/icons/app_icon_256x256.png")

# 验证图标文件存在
if(NOT EXISTS "${APP_ICON_MACOS}")
    message(WARNING "macOS icon not found: ${APP_ICON_MACOS}")
else()
    message(STATUS "Using macOS icon: ${APP_ICON_MACOS}")
endif()

if(NOT EXISTS "${APP_ICON_WINDOWS}")
    message(WARNING "Windows icon not found: ${APP_ICON_WINDOWS}")
else()
    message(STATUS "Using Windows icon: ${APP_ICON_WINDOWS}")
endif()

if(NOT EXISTS "${APP_ICON_PNG}")
    message(WARNING "PNG icon not found: ${APP_ICON_PNG}")
else()
    message(STATUS "Using PNG icon: ${APP_ICON_PNG}")
endif()

# 安装组件配置
set(CPACK_COMPONENTS_ALL "Application;Resources;Documentation")
set(CPACK_COMPONENT_APPLICATION_DISPLAY_NAME "PerfxAgent-ASR Application")
set(CPACK_COMPONENT_RESOURCES_DISPLAY_NAME "Resources and Assets")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")

# 组件依赖关系
set(CPACK_COMPONENT_APPLICATION_DEPENDS "Resources")
set(CPACK_COMPONENT_DOCUMENTATION_DEPENDS "Application")

# 添加子目录
add_subdirectory(src)

# 构建 ixwebsocket 库
add_subdirectory(third_party/ixwebsocket)

# macOS Bundle 配置
set(MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.perfx.agent")
set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_BUNDLE_VERSION "${PERFXAGENT_VERSION}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PERFXAGENT_VERSION}")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PERFXAGENT_VERSION}")
set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} Version ${PERFXAGENT_VERSION}")
set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 PerfX. All rights reserved.")
set(MACOSX_BUNDLE_ICON_FILE "app_icon.icns")

# 安装目标配置
install(TARGETS perfxagent-app
    BUNDLE DESTINATION .
    COMPONENT Application
)

# 安装资源文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/
    DESTINATION PerfxAgent-ASR.app/Contents/Resources
    COMPONENT Resources
    FILES_MATCHING 
    PATTERN "*.png"
    PATTERN "*.jpg"
    PATTERN "*.svg"
    PATTERN "*.qss"
    PATTERN "*.json"
)

# 安装配置文件
install(FILES ${CMAKE_SOURCE_DIR}/config.json
    DESTINATION PerfxAgent-ASR.app/Contents/Resources
    COMPONENT Resources
)

# 安装文档
install(FILES ${CMAKE_SOURCE_DIR}/README.md
    DESTINATION PerfxAgent-ASR.app/Contents/Resources
    COMPONENT Documentation
    RENAME "README.txt"
)

# 安装许可证
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION PerfxAgent-ASR.app/Contents/Resources
    COMPONENT Documentation
    RENAME "LICENSE.txt"
)

# 安装示例文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/sample/
    DESTINATION PerfxAgent-ASR.app/Contents/Resources/samples
    COMPONENT Resources
    FILES_MATCHING PATTERN "*.wav"
    PATTERN "*.mp3"
    PATTERN "*.flac"
)

# 创建应用程序快捷方式
install(CODE "
    execute_process(
        COMMAND ln -sf /Applications/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
        /usr/local/bin/perfxagent-asr
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(WARNING \"Failed to create command line shortcut\")
    endif()
" COMPONENT Application)

# 设置应用程序权限
install(CODE "
    execute_process(
        COMMAND chmod +x \${CMAKE_INSTALL_PREFIX}/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(WARNING \"Failed to set executable permissions\")
    endif()
" COMPONENT Application)

# 链接库配置
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${PORTAUDIO_PREFIX}/lib -L${OPUS_PREFIX}/lib -L/opt/homebrew/lib") 