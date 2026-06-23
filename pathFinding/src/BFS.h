#pragma once

#include "Maze.h"

#include <queue>
#include <vector>

class BFS
{
public:

    void initialize(const Maze& maze);

    // Egy BFS lépés
    bool step(const Maze& maze);

    bool finished() const { return finishedFlag; }
    bool pathFound() const { return foundFlag; }

    const std::vector<int>& getDistances() const
    {
        return dist;
    }

    const std::vector<int>& getPath() const
    {
        return path;
    }

private:

    void reconstructPath(int goal);

private:

    std::queue<int> frontier;

    std::vector<int> dist;
    std::vector<int> parent;
    std::vector<int> path;

    int startIndex = -1;
    int goalIndex = -1;

    int width = 0;
    int height = 0;

    bool finishedFlag = false;
    bool foundFlag = false;
};