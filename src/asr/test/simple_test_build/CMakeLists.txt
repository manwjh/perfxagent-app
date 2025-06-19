cmake_minimum_required(VERSION 3.10)
project(simple_asr_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_DEFAULT_MAJOR_VERSION 6)

# 查找 Qt6 组件
find_package(Qt6 COMPONENTS Core Network WebSockets REQUIRED)

# 创建测试程序
add_executable(simple_asr_test 
    simple_asr_test.cpp
    asr_qt_client.cpp
)

# 链接 Qt6 库
target_link_libraries(simple_asr_test 
    Qt6::Core 
    Qt6::Network 
    Qt6::WebSockets
)

# 设置包含目录
target_include_directories(simple_asr_test PRIVATE 
    ../../include
    ../..
)

# 添加 zlib 依赖
find_package(ZLIB REQUIRED)
target_link_libraries(simple_asr_test ZLIB::ZLIB) 