#include "BFS.h"
#include <iostream>

void BFS::initialize(const Maze& maze)
{
    width = maze.getWidth();
    height = maze.getHeight();

    int size = width * height;

    dist.assign(size, -1);
    parent.assign(size, -1);
    path.clear();

    while (!frontier.empty()) frontier.pop();

    finishedFlag = false;
    foundFlag = false;

    startIndex = -1;
    goalIndex = -1;

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int idx = y * width + x;

            if (maze.get(x, y) == START)
                startIndex = idx;

            if (maze.get(x, y) == GOAL)
                goalIndex = idx;
        }

    dist[startIndex] = 0;
    frontier.push(startIndex);
}

bool BFS::step(const Maze& maze)
{
    if (finishedFlag) return true;
    if (frontier.empty()) return finishedFlag = true;

    int cur = frontier.front();
    frontier.pop();


    if (cur == goalIndex)
    {
        reconstructPath(cur);
        foundFlag = true;
        finishedFlag = true;
        return true;
    }

    int cx = cur % width;
    int cy = cur / width;

    const int dx[4] = { 1,-1,0,0 };
    const int dy[4] = { 0,0,1,-1 };

    for (int i = 0; i < 4; i++)
    {
        int nx = cx + dx[i];
        int ny = cy + dy[i];

        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            continue;

        if (maze.get(nx, ny) == WALL)
            continue;

        int ni = ny * width + nx;

        if (dist[ni] == -1)
        {
            dist[ni] = dist[cur] + 1;
            parent[ni] = cur;
            frontier.push(ni);
        }
    }

    return false;
}

void BFS::reconstructPath(int goal)
{
    path.clear();
    int cur = goal;

    while (cur != -1)
    {
        path.push_back(cur);
        cur = parent[cur];
    }
}