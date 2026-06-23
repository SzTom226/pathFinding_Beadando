#include "Renderer.h"
#include "Maze.h"

int main()
{
    Maze maze(20, 20);
    maze.generateSimple();

    Renderer renderer(800, 800, 20, 20);

    if (!renderer.init())
        return -1;

    while (!renderer.shouldClose())
    {
        renderer.beginFrame();
        renderer.drawMaze(maze);
        renderer.endFrame();
    }
    return 0;
}