cmake_minimum_required(VERSION 3.20)

project(pathFinding)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

add_executable(pathFinding
    src/main.cpp
    src/Maze.cpp
    src/BFS.cpp
    src/Renderer.cpp
    src/OpenCLManager.cpp
    src/GPUBFS.cpp
)

# OpenCL
target_include_directories(pathFinding PRIVATE
    C:/dev/OpenCL-SDK/install/include
)

target_link_directories(pathFinding PRIVATE
    C:/dev/OpenCL-SDK/install/lib
)

target_link_libraries(pathFinding PRIVATE
    OpenCL
)

# Saját headerek
target_include_directories(pathFinding PRIVATE
    src
)

# Kernel fájlok elérési útja (bfs.cl és color.cl)
target_compile_definitions(pathFinding PRIVATE
    KERNEL_DIR="${CMAKE_SOURCE_DIR}/kernels/"
)

# GLAD
add_library(glad
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

# GLFW
add_subdirectory(external/glfw)

target_link_libraries(pathFinding PRIVATE
    glfw
    glad
    OpenGL::GL
)

# Windows: CL/GL interophoz szükséges
if(WIN32)
    target_link_libraries(pathFinding PRIVATE
        gdi32
        user32
    )
endif()