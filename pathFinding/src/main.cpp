#include "Renderer.h"
#include "Maze.h"
#include "BFS.h"

int main()
{
    Maze maze(20, 20);
    maze.generateRandom();

    BFS bfs;
    bfs.initialize(maze);

    Renderer renderer(800, 800, 20, 20);

    if (!renderer.init())
        return -1;

    // animáció időzítése
    const double bfsStepInterval = 0.03;   // mp / BFS lépés
    const double pathStepInterval = 0.04;  // mp / útvonal-cella felfedés

    double lastBfsStepTime = glfwGetTime();
    double lastPathStepTime = glfwGetTime();
    int pathRevealCount = 0;

    while (!renderer.shouldClose())
    {
        double now = glfwGetTime();

        if (!bfs.finished())
        {
            if (now - lastBfsStepTime >= bfsStepInterval)
            {
                lastBfsStepTime = now;
                bfs.step(maze);
            }
        }
        else if (bfs.pathFound())
        {
            int pathLen = (int)bfs.getPath().size();
            if (pathRevealCount < pathLen && now - lastPathStepTime >= pathStepInterval)
            {
                lastPathStepTime = now;
                pathRevealCount++;
            }
        }

        renderer.beginFrame();
        renderer.drawMaze(maze, bfs, pathRevealCount);
        renderer.endFrame();
    }
    return 0;
}