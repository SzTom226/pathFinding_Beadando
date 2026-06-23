#pragma once

#include "Maze.h"
#include <vector>

class BFS {
public:
	bool findPath(const Maze& maze);

	const std::vector<int>& getPath() const { return path; }
	const std::vector<int>& getDistances() const { return dist; }
private:
	std::vector<int> dist;
	std::vector<int> parent;
	std::vector<int> path;
};