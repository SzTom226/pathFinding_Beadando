#include <CL/cl.h>
#include <iostream>
#include "Maze.h"
#include "BFS.h"

int main()
{
    Maze maze(20, 20);
    maze.generateSimple();

    BFS bfs;

    if (!bfs.findPath(maze))
    {
        std::cout << "No path found\n";
        return 0;
    }

    const auto& path = bfs.getPath();

    int w = maze.getWidth();
    int h = maze.getHeight();

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int index = y * w + x;

            bool onPath = false;

            for (int p : path)
            {
                if (p == index)
                {
                    onPath = true;
                    break;
                }
            }

            uint8_t cell = maze.get(x, y);

            if (cell == WALL)
                std::cout << '#';
            else if (cell == START)
                std::cout << 'S';
            else if (cell == GOAL)
                std::cout << 'G';
            else if (onPath)
                std::cout << '*';
            else
                std::cout << '.';
        }

        std::cout << '\n';
    }

    return 0;
}