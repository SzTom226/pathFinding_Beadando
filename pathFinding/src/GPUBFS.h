#pragma once
#include "Maze.h"
#include "OpenCLManager.h"

class GPUBFS {
public:
	bool initialize(OpenCLManager& cl, const Maze& maze);
	bool step();
	const std::vector<int>& getDisctances() const { return hostDistance; }
private:
	int width = 0;
	int height = 0;
	OpenCLManager* clManager = nullptr;

	cl_program program = nullptr;
	cl_kernel kernel = nullptr;

	cl_mem mazeBuffer = nullptr;
	cl_mem distanceBuffer = nullptr;
	cl_mem frontierBuffer = nullptr;
	cl_mem nextFrontierBuffer = nullptr;
	cl_mem nextCountBuffer = nullptr;

	std::vector<int> hostDistance;
	int currentDistance = 0;
};