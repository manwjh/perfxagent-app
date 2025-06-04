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

# 已移除 websocket_example.cpp 及相关 target 