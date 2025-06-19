cmake_minimum_required(VERSION 3.14)

project(camera_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Add the camera library
add_library(perfx_camera STATIC
    ${CMAKE_SOURCE_DIR}/src/camera/camera_device.cpp
    ${CMAKE_SOURCE_DIR}/src/camera/camera_manager.cpp
)

# Set include directories for the camera library
target_include_directories(perfx_camera PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Link dependencies for the camera library
target_link_libraries(perfx_camera PUBLIC
    ${OpenCV_LIBS}
    Threads::Threads
)

# Add the example executable
add_executable(camera_example
    camera_example.cpp
)

# Link the example with required libraries
target_link_libraries(camera_example
    perfx_camera
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${OpenCV_LIBS}
)

# Set include directories for the example
target_include_directories(camera_example PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
) 