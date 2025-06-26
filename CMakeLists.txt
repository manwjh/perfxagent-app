cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)
cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0002 NEW)

cmake_minimum_required(VERSION 3.10)

# =============================================================================
# 平台检测和配置
# =============================================================================

# 检测平台
if(APPLE)
    set(PLATFORM "macos")
    message(STATUS "Detected platform: macOS")
elseif(WIN32)
    set(PLATFORM "windows")
    message(STATUS "Detected platform: Windows")
elseif(UNIX)
    set(PLATFORM "linux")
    message(STATUS "Detected platform: Linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# =============================================================================
# 包含通用配置
# =============================================================================

include(cmake/platforms/common.cmake)

# =============================================================================
# 包含平台特定配置
# =============================================================================

if(PLATFORM STREQUAL "macos")
    include(cmake/platforms/macos.cmake)
    include(cmake/packaging/macos_package.cmake)
elseif(PLATFORM STREQUAL "windows")
    include(cmake/platforms/windows.cmake)
    include(cmake/packaging/windows_package.cmake)
elseif(PLATFORM STREQUAL "linux")
    # Linux配置将在后续实现
    message(STATUS "Linux configuration will be implemented in next phase")
endif()

message(STATUS "Platform configuration completed for: ${PLATFORM}")

# =============================================================================
# 包含子目录
# =============================================================================

# 检查target是否已存在，避免重复创建
get_property(IXWEBSOCKET_EXISTS TARGET ixwebsocket PROPERTY TYPE SET)
if(NOT IXWEBSOCKET_EXISTS)
    add_subdirectory(third_party/ixwebsocket build_ixwebsocket)
endif()

get_property(SRC_EXISTS TARGET perfxagent-app PROPERTY TYPE SET)
if(NOT SRC_EXISTS)
    add_subdirectory(src build_src)
endif() 