#include "BFS.h"

void BFS::initialize(const Maze& maze)
{
    width = maze.getWidth();
    height = maze.getHeight();

    dist.assign(width * height, -1);
    parent.assign(width * height, -1);
    path.clear();

    while (!frontier.empty())
    {
        frontier.pop();
    }

    finishedFlag = false;
    foundFlag = false;

    startIndex = -1;
    goalIndex = -1;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = y * width + x;

            if (maze.get(x, y) == START)
            {
                startIndex = idx;
            }

            if (maze.get(x, y) == GOAL)
            {
                goalIndex = idx;
            }
        }
    }

    if (startIndex == -1 || goalIndex == -1)
    {
        finishedFlag = true;
        return;
    }

    dist[startIndex] = 0;
    frontier.push(startIndex);
}

bool BFS::step(const Maze& maze)
{
    if (finishedFlag)
    {
        return true;
    }

    if (frontier.empty())
    {
        finishedFlag = true;
        return true;
    }

    int current = frontier.front();
    frontier.pop();

    if (current == goalIndex)
    {
        reconstructPath(goalIndex);

        foundFlag = true;
        finishedFlag = true;

        return true;
    }

    int cx = current % width;
    int cy = current / width;

    const int dirs[4][2] =
    {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0,-1 }
    };

    for (const auto& d : dirs)
    {
        int nx = cx + d[0];
        int ny = cy + d[1];

        if (nx < 0 || ny < 0 ||
            nx >= width || ny >= height)
        {
            continue;
        }

        if (maze.get(nx, ny) == WALL)
        {
            continue;
        }

        int neighbourIndex = ny * width + nx;

        if (dist[neighbourIndex] == -1)
        {
            dist[neighbourIndex] = dist[current] + 1;
            parent[neighbourIndex] = current;

            frontier.push(neighbourIndex);
        }
    }

    return false;
}

void BFS::reconstructPath(int goal)
{
    path.clear();

    int current = goal;

    while (current != -1)
    {
        path.push_back(current);
        current = parent[current];
    }
}