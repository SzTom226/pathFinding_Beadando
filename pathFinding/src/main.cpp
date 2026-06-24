#include "Renderer.h"
#include "Maze.h"
#include "BFS.h"
#include "GPUBFS.h"
#include "BFSBase.h"
#include "OpenCLManager.h"

#include <GLFW/glfw3.h>
#include <iostream>

enum class Mode
{
    CPU,
    GPU
};

int main()
{
    OpenCLManager clManager;

    if (!clManager.initialize())
    {
        std::cout << "OpenCL init failed\n";
        return -1;
    }

    Maze maze(20, 20);
    maze.generateRandom();

    Renderer renderer(800, 800, maze.getWidth(), maze.getHeight());

    if (!renderer.init())
    {
        std::cout << "Renderer init failed\n";
        return -1;
    }

    Mode mode = Mode::GPU;

    BFSBase* bfs = nullptr;

    if (mode == Mode::CPU)
    {
        bfs = new BFS();
        std::cout << "Using CPU BFS\n";
    }
    else
    {
        GPUBFS* gpu = new GPUBFS();
        gpu->setOpenCL(clManager);
        bfs = gpu;
        std::cout << "Using GPU BFS\n";
    }

    bfs->initialize(maze);

    const double bfsStepInterval = 0.03;
    const double pathStepInterval = 0.04;

    double lastBfsStepTime = glfwGetTime();
    double lastPathStepTime = glfwGetTime();
    int pathRevealCount = 0;

    while (!renderer.shouldClose())
    {
        double now = glfwGetTime();

        if (!bfs->finished())
        {
            if (now - lastBfsStepTime >= bfsStepInterval)
            {
                lastBfsStepTime = now;
                bfs->step(maze);
            }
        }

        if (bfs->finished() && bfs->pathFound())
        {
            int pathLen = (int)bfs->getPath().size();

            if (pathRevealCount < pathLen &&
                now - lastPathStepTime >= pathStepInterval)
            {
                lastPathStepTime = now;
                pathRevealCount++;
            }
        }

        renderer.beginFrame();
        renderer.drawMaze(maze, *bfs, pathRevealCount);
        renderer.endFrame();
    }

    delete bfs;

    return 0;
}