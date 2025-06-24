# 设置 Qt 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置 MOC 包含路径
set(CMAKE_AUTOMOC_PATH_PREFIX "")

# 添加音频库
add_library(perfx_audio STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/file_importer.cpp
    ${CMAKE_SOURCE_DIR}/include/audio/audio_manager.h
    ${CMAKE_SOURCE_DIR}/include/audio/audio_device.h
    ${CMAKE_SOURCE_DIR}/include/audio/audio_thread.h
    ${CMAKE_SOURCE_DIR}/include/audio/audio_processor.h
    ${CMAKE_SOURCE_DIR}/include/audio/audio_converter.h
    ${CMAKE_SOURCE_DIR}/include/audio/audio_types.h
)

# 设置音频库的包含目录
target_include_directories(perfx_audio PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/perfx_audio_autogen/include
    ${PortAudio_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${Qt6Concurrent_INCLUDE_DIRS}
)

# 设置 MOC 特定选项
set_target_properties(perfx_audio PROPERTIES
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include"
)

# 链接音频库的依赖
target_link_libraries(perfx_audio PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    ${PortAudio_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${OGG_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

# 添加 IXWebSocket 版本的 ASR 库
add_library(perfx_asr_client STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/asr/asr_client.cpp
    ${CMAKE_SOURCE_DIR}/include/asr/asr_client.h
)

target_include_directories(perfx_asr_client PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket
    ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(perfx_asr_client PUBLIC
    ixwebsocket
    nlohmann_json::nlohmann_json
    z
)

# 添加 ASR 管理模块库
add_library(perfx_asr_manager STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/asr/asr_manager.cpp
    ${CMAKE_SOURCE_DIR}/include/asr/asr_manager.h
)

target_include_directories(perfx_asr_manager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(perfx_asr_manager PUBLIC
    perfx_asr_client
    nlohmann_json::nlohmann_json
)

# 添加主应用程序
set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/ui/main_window.h
    ${CMAKE_SOURCE_DIR}/include/ui/audio_to_text_window.h
    ${CMAKE_SOURCE_DIR}/include/ui/realtime_audio_to_text_window.h
    ${CMAKE_SOURCE_DIR}/include/logic/realtime_transcription_controller.h
    ${CMAKE_SOURCE_DIR}/include/ui/app_icon_button.h
)

# 确保 icon 文件被加到 bundle
set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/app_icon.icns")
set(SOURCES
    main.cpp
    ui/main_window.cpp
    ui/audio_to_text_window.cpp
    ui/realtime_audio_to_text_window.cpp
    logic/realtime_transcription_controller.cpp
    ui/app_icon_button.cpp
    ${HEADERS}
    ${APP_ICON_MACOS}
)
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

add_executable(perfxagent-app
    ${SOURCES}
)

# 设置主应用程序的包含目录
target_include_directories(perfxagent-app PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/perfxagent-app_autogen/include
    ${PortAudio_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6WebEngineWidgets_INCLUDE_DIRS}
    ${Qt6WebSockets_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${Qt6Concurrent_INCLUDE_DIRS}
    ${Qt6PrintSupport_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${BOOST_INCLUDEDIR}
    ${nlohmann_json_INCLUDE_DIRS}
)

# 链接主应用程序的依赖
target_link_libraries(perfxagent-app PRIVATE
    perfx_audio
    perfx_asr_client
    perfx_asr_manager
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::WebEngineWidgets
    Qt6::WebSockets
    Qt6::Concurrent
    Qt6::PrintSupport
    nlohmann_json::nlohmann_json
    portaudio
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${OGG_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

# macOS Bundle 配置
set_target_properties(perfxagent-app PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.perfx.agent.asr"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PERFXAGENT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PERFXAGENT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
    MACOSX_BUNDLE_ICON_FILE "app_icon.icns"
    OUTPUT_NAME "PerfxAgent-ASR"
) 