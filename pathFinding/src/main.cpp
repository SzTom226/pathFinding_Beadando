#include "Renderer.h"
#include "Maze.h"
#include "BFS.h"
#include "GPUBFS.h"
#include "BFSBase.h"
#include "OpenCLManager.h"

#include <GLFW/glfw3.h>
#include <iostream>

int main()
{
    // 1) Labirintus
    Maze maze(40, 40);
    maze.generateRandom();

    // 2) Renderer + OpenGL context
    Renderer renderer(800, 800, maze.getWidth(), maze.getHeight());
    if (!renderer.init()) {
        std::cout << "Renderer init failed\n";
        return -1;
    }

    // 3) OpenCL – FONTOS: az OpenGL context létrehozása UTÁN,
    //    hogy a GL sharing properties érvényesek legyenek
    OpenCLManager clManager;
    if (!clManager.initialize()) {
        std::cout << "OpenCL init failed\n";
        return -1;
    }

    // 4) Interop textúra: GL textúra + CL image ugyanaz a GPU memória
    if (!renderer.initInterop(clManager.getContext(), clManager.getQueue())) {
        std::cout << "Interop init failed\n";
        return -1;
    }

    // 5) GPUBFS
    GPUBFS gpuBfs;
    gpuBfs.setOpenCL(clManager);
    gpuBfs.initialize(maze);

    const double bfsStepInterval = 0.03;
    const double pathStepInterval = 0.01;

    double lastBfsStep = glfwGetTime();
    double lastPathStep = glfwGetTime();
    int    pathReveal = 0;

    while (!renderer.shouldClose())
    {
        double now = glfwGetTime();

        // BFS lépés
        if (!gpuBfs.finished() && now - lastBfsStep >= bfsStepInterval) {
            lastBfsStep = now;
            gpuBfs.step(maze);
        }

        // Útvonal animáció
        if (gpuBfs.finished() && gpuBfs.pathFound()) {
            int pathLen = (int)gpuBfs.getPath().size();
            if (pathReveal < pathLen && now - lastPathStep >= pathStepInterval) {
                lastPathStep = now;
                pathReveal++;
            }
        }

        // Color kernel: distBuf + pathBuf → megosztott GL textúra
        // CPU roundtrip nélkül, GPU memórián belül
        gpuBfs.updateColorTexture(renderer.getColorImage(), pathReveal);

        // Rajzolás: 1 draw call, fullscreen quad
        renderer.beginFrame();
        renderer.drawMazeInterop();
        renderer.endFrame();
    }

    return 0;
}