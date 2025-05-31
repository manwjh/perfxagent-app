# 添加音频库
add_library(perfx_audio STATIC
    audio/audio_manager.cpp
    audio/audio_device.cpp
    audio/audio_thread.cpp
    audio/audio_config.cpp
)

# 设置音频库的包含目录
target_include_directories(perfx_audio PUBLIC
    ${CMAKE_SOURCE_DIR}/../include
    ${PortAudio_INCLUDE_DIRS}
    ${Qt6Core_INCLUDE_DIRS}
)

# 链接音频库的依赖
target_link_libraries(perfx_audio PUBLIC
    Qt6::Core
    ${PortAudio_LIBRARIES}
)

# 添加主应用程序
add_executable(perfxagent-app
    main.cpp
    ui/main_window.cpp
    ui/agent_zoo.cpp
    ui/device_settings.cpp
    ui/hello_window.cpp
    core/config_manager.cpp
    connection/websocket_client.cpp
    connection/websocket_message.cpp
    connection/thread_pool.cpp
    connection/connection_manager.cpp
)

# 设置主应用程序的包含目录
target_include_directories(perfxagent-app PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
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
    ${nlohmann_json_INCLUDE_DIRS}
)

# 链接主应用程序的依赖
target_link_libraries(perfxagent-app PRIVATE
    perfx_audio
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