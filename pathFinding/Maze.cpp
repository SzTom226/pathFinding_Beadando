#include "Maze.h"

Maze::Maze(int w, int h)
    : width(w), height(h), grid(w* h, WALL)
{
}

uint8_t Maze::get(int x, int y) const
{
    return grid[index(x, y)];
}

void Maze::set(int x, int y, uint8_t value)
{
    grid[index(x, y)] = value;
}

int Maze::index(int x, int y) const
{
    return y * width + x;
}

void Maze::generateSimple()
{
    // alap: minden EMPTY
    for (auto& cell : grid)
        cell = EMPTY;

    // keret fal
    for (int x = 0; x < width; x++)
    {
        set(x, 0, WALL);
        set(x, height - 1, WALL);
    }

    for (int y = 0; y < height; y++)
    {
        set(0, y, WALL);
        set(width - 1, y, WALL);
    }

    // belső akadályok (egyszerű minta)
    for (int x = 2; x < width - 2; x += 2)
    {
        for (int y = 2; y < height - 2; y++)
        {
            if ((x + y) % 3 == 0)
                set(x, y, WALL);
        }
    }

    // start + goal
    set(1, 1, START);
    set(width - 2, height - 2, GOAL);
}