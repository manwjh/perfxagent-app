# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 如果是独立编译，需要查找依赖包
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 查找依赖包
    find_package(PortAudio 19.7.0 REQUIRED)
    find_package(Opus REQUIRED)
    find_package(SAMPLERATE REQUIRED)
    find_package(nlohmann_json 3.11.3 REQUIRED)
    find_package(OpenCV REQUIRED)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswresample
    )
endif()

# 添加音频示例
# add_subdirectory(audio)

# 添加相机示例
# add_subdirectory(camera)

# 添加ASR使用示例
add_executable(asr_usage_example
    asr_usage_example.cpp
)

# 设置ASR示例的包含目录
target_include_directories(asr_usage_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/asr_usage_example_autogen/include
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

# 链接ASR示例的依赖
target_link_libraries(asr_usage_example PRIVATE
    perfx_asr_qt
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    z
) 