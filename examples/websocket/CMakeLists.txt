cmake_minimum_required(VERSION 3.10)
project(websocket_example)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果是独立编译，需要查找依赖包
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 查找依赖包
    find_package(Boost REQUIRED COMPONENTS system thread)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    find_package(nlohmann_json 3.11.3 REQUIRED)
endif()

# 添加可执行文件
add_executable(websocket_example websocket_example.cpp)

# 包含头文件目录
target_include_directories(websocket_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/connection
)

# 链接必要的库
target_link_libraries(websocket_example PRIVATE
    perfx_connection
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Darwin>:${BOOST_LIBRARYDIR}/libboost_url.dylib>
)

# 设置输出目录
set_target_properties(websocket_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 