#include <CL/cl.h>
#include <iostream>
#include "Maze.h"

int main() {
    
    int width = 20;
    int height = 20;

    Maze maze(width, height);

    // labirintus generálása
    maze.generateSimple();

    // debug kiírás konzolra
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8_t cell = maze.get(x, y);

            switch (cell)
            {
            case 0: std::cout << "#"; break; // WALL
            case 1: std::cout << "."; break; // EMPTY
            case 2: std::cout << "S"; break; // START
            case 3: std::cout << "G"; break; // GOAL
            }
        }
        std::cout << "\n";
    }
    return 0;
}