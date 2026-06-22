cmake_minimum_required(VERSION 3.20)

project(pathFinding)

set(CMAKE_CXX_STANDARD 17)

add_executable(pathFinding main.cpp "Maze.cpp")

target_include_directories(pathFinding PRIVATE
	C:/dev/OpenCL-SDK/install/include	
)

target_link_directories(pathFinding PRIVATE
	c:/dev/OpenCL-SDK/install/lib
)

target_link_libraries(pathFinding PRIVATE
	OpenCL
)