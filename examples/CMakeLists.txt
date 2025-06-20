# ASR Examples

# ASR IXWebSocket 示例（支持 HTTP 响应头）
add_executable(asr_ixwebsocket_example asr_ixwebsocket_example.cpp)
target_link_libraries(asr_ixwebsocket_example 
    ixwebsocket
    nlohmann_json::nlohmann_json
    z
)

target_include_directories(asr_ixwebsocket_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket
)

set_target_properties(asr_ixwebsocket_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ASR 简化示例（用户层调用示例）
add_executable(asr_simple_example asr_simple_example.cpp)
target_link_libraries(asr_simple_example 
    perfx_asr_manager
    perfx_asr_client
    ixwebsocket
    nlohmann_json::nlohmann_json
    z
)

target_include_directories(asr_simple_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket
)

set_target_properties(asr_simple_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 