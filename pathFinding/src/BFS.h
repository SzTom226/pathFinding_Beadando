#pragma once

#include "Maze.h"
#include "BFSBase.h"
#include <queue>

class BFS : public BFSBase
{
public:
    void initialize(const Maze& maze) override;
    bool step(const Maze& maze) override;

    bool finished() const override { return finishedFlag; }
    bool pathFound() const override { return foundFlag; }

    const std::vector<int>& getDistances() const override { return dist; }
    const std::vector<int>& getPath() const override { return path; }

private:
    void reconstructPath(int goal);

private:
    std::queue<int> frontier;

    std::vector<int> dist;
    std::vector<int> parent;
    std::vector<int> path;

    int width = 0;
    int height = 0;

    int startIndex = -1;
    int goalIndex = -1;

    bool finishedFlag = false;
    bool foundFlag = false;
};