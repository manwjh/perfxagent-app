cmake_minimum_required(VERSION 3.15)

project(perfxagent-app VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add Homebrew paths for macOS
if(APPLE)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew")
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PortAudio REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PortAudio_INCLUDE_DIRS}
)

file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS 
    "include/*.h"
)

add_executable(perfxagent-app ${SOURCES} ${HEADERS})

target_link_libraries(perfxagent-app PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${PortAudio_LIBRARIES}
)

install(TARGETS perfxagent-app
    RUNTIME DESTINATION bin
) 