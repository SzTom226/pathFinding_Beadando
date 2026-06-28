#include "Renderer.h"
#include "Maze.h"
#include "BFS.h"
#include "GPUBFS.h"
#include "BFSBase.h"
#include "OpenCLManager.h"
#include "Profiler.h"

#include <GLFW/glfw3.h>
#include <iostream>

// ----------------------------------------------------------------------
// A keresési algoritmus végrehajtási módja: a felhasználó választja ki,
// hogy a BFS a CPU-n (egyszálú, std::queue-alapú BFS osztály) vagy a
// GPU-n (OpenCL, párhuzamos hullámfront-kiterjesztésű GPUBFS osztály)
// fusson le.
// ----------------------------------------------------------------------
enum class BFSMode { CPU, GPU };

// Konzolon kéri be a felhasználótól, hogy melyik módot szeretné –
// addig kérdez, amíg érvényes választ ('c'/'g') nem kap.
static BFSMode askBFSMode()
{
    while (true)
    {
        std::cout << "Melyik BFS implementaciot inditsuk?\n";
        std::cout << "  [c] CPU (egyszalu, std::queue)\n";
        std::cout << "  [g] GPU (OpenCL, parhuzamos hullamfront)\n";
        std::cout << "Valasz: ";

        char choice = 0;
        std::cin >> choice;

        if (choice == 'c' || choice == 'C') return BFSMode::CPU;
        if (choice == 'g' || choice == 'G') return BFSMode::GPU;

        std::cout << "Ervenytelen valasz, probald ujra.\n\n";
    }
}

int main()
{
    // 0) A felhasználó kiválasztja, hogy a BFS keresés CPU-n vagy GPU-n fusson
    BFSMode mode = askBFSMode();

    // 1) Labirintus
    Maze maze(128, 128);
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

    // 5) A vizualizációhoz MINDIG a GPUBFS objektum buffereit/kernelét
    //    használjuk (hiszen a Renderer a GL/CL interop textúrára épül).
    //    GPU módban ez az objektum végzi magát a keresést is; CPU módban
    //    csak "kirajzoló hídként" szolgál: a CPU-s eredményt minden
    //    lépés után feltöltjük rá (lásd lejjebb, uploadVisualizationData).
    GPUBFS gpuBfs;
    gpuBfs.setOpenCL(clManager);
    gpuBfs.initialize(maze);

    // CPU-s implementáció – csak akkor használjuk a tényleges kereséshez,
    // ha a felhasználó a CPU módot választotta, de mindig inicializáljuk,
    // hogy ne kelljen elágazni az initialize()-on.
    BFS cpuBfs;
    cpuBfs.initialize(maze);

    // A BFSBase interfészen keresztül a főciklusnak nem kell tudnia,
    // melyik konkrét implementációt futtatja – csak a step()/finished()/
    // pathFound()/getPath()/getDistances() metódusokat hívja rajta.
    BFSBase* bfsAlgo = (mode == BFSMode::GPU)
        ? static_cast<BFSBase*>(&gpuBfs)
        : static_cast<BFSBase*>(&cpuBfs);

    std::cout << ((mode == BFSMode::GPU) ? "GPU" : "CPU") << " BFS inditva.\n";

    // CPU méréshez
    Stopwatch cpuSw;
    Profiler cpuProfiler;

    const double bfsStepInterval = 0.03;
    const double pathStepInterval = 0.01;

    double lastBfsStep = glfwGetTime();
    double lastPathStep = glfwGetTime();
    int    pathReveal = 0;
    bool statsPrinted = false;

    while (!renderer.shouldClose())
    {
        double now = glfwGetTime();

        // BFS lépés – mindig a kiválasztott algoritmuson (CPU vagy GPU) keresztül
        if (!bfsAlgo->finished() && now - lastBfsStep >= bfsStepInterval) {
            lastBfsStep = now;
            
            if (mode == BFSMode::CPU) {
                cpuSw.start();
                bfsAlgo->step(maze);
                cpuSw.stop(cpuProfiler.cpuTotalMs);
                cpuProfiler.cpuSteps++;

                // Vizualizációs adatok feltöltése GPU-ra
                gpuBfs.uploadVisualizationData(bfsAlgo->getDistances(), bfsAlgo->getPath());
            }
            else {
                bfsAlgo->step(maze);
            }
        }

        // Útvonal animáció
        if (bfsAlgo->finished() && bfsAlgo->pathFound()) {
            int pathLen = (int)bfsAlgo->getPath().size();
            if (pathReveal < pathLen && now - lastPathStep >= pathStepInterval) {
                lastPathStep = now;
                pathReveal++;
            }

            // Statisztika kiírása egyszer, mikor a keresés befejeződött
            if (!statsPrinted)
            {
                statsPrinted = true;

                if (mode == BFSMode::GPU)
                {
                    gpuBfs.getProfiler().printGPU();
                }
                else
                {
                    cpuProfiler.printCPU();
                }

                // Mindkét módban GPU-n is lefuttatjuk a CPU BFS-t
                // (csak mérési célból, ha CPU módban vagyunk),
                // hogy speedupot lehessen számítani.
                // Megjegyzés: ha GPU módban vagyunk, a CPU időt nem mértük,
                // ezért csak GPU módban jelezzük a kernel vs transzfer arányt.
                if (mode == BFSMode::GPU)
                {
                    const Profiler& gp = gpuBfs.getProfiler();
                    std::cout << "\n========== Kernel vs Transzfer arany ==========\n";
                    std::cout << std::fixed << std::setprecision(1);
                    if (gp.gpuTotalMs > 0) {
                        std::cout << "  Kernel:    "
                            << (gp.gpuKernelMs / gp.gpuTotalMs * 100.0)
                            << "%\n";
                        std::cout << "  H->D:      "
                            << (gp.gpuH2DMs / gp.gpuTotalMs * 100.0)
                            << "%\n";
                        std::cout << "  D->H:      "
                            << (gp.gpuD2HMs / gp.gpuTotalMs * 100.0)
                            << "%\n";
                        std::cout << "  Egyeb:     "
                            << ((gp.gpuTotalMs - gp.gpuKernelMs
                                - gp.gpuH2DMs - gp.gpuD2HMs)
                                / gp.gpuTotalMs * 100.0)
                            << "%\n";
                    }
                }
                std::cout << "\n(Az ablak bezarasaig a vizualizacio fut tovabb.)\n";
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