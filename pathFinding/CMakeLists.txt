cmake_minimum_required(VERSION 3.20)

project(pathFinding)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

add_executable(pathFinding src/main.cpp src/Maze.cpp src/BFS.cpp src/Renderer.cpp)

target_include_directories(pathFinding PRIVATE
	C:/dev/OpenCL-SDK/install/include	
)

target_link_directories(pathFinding PRIVATE
	c:/dev/OpenCL-SDK/install/lib
)

target_link_libraries(pathFinding PRIVATE
	OpenCL
)

target_include_directories(pathFinding PRIVATE
    src
)

add_library(glad
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

add_subdirectory(external/glfw)

target_link_libraries(pathFinding PRIVATE
    glfw
    glad
    OpenGL::GL
)