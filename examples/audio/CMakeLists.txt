cmake_minimum_required(VERSION 3.10)
project(audio_example)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果是独立编译，需要查找依赖包
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 查找依赖包
    find_package(PortAudio 19.7.0 REQUIRED)
    find_package(Opus REQUIRED)
    find_package(SAMPLERATE REQUIRED)
    find_package(nlohmann_json 3.11.3 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswresample
    )
endif()

# 添加可执行文件
add_executable(audio_example audio_example.cpp)
add_executable(pa_record_example pa_record_example.cpp)

# 包含头文件目录
target_include_directories(audio_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/audio
    ${PortAudio_INCLUDE_DIRS}
    ${Opus_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
)
target_include_directories(pa_record_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
)

# 链接必要的库
target_link_libraries(audio_example PRIVATE
    perfx_audio
    ${PortAudio_LIBRARIES}
    ${Opus_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
    nlohmann_json::nlohmann_json
    PkgConfig::FFMPEG
    ogg
)
target_link_libraries(pa_record_example PRIVATE
    ${PortAudio_LIBRARIES}
)

# 设置输出目录
set_target_properties(audio_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(pa_record_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 