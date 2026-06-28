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
    Maze maze(64, 64);
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
    //    GPU módban ez az objektum végzi magát a vizuális animációt;
    //    CPU módban csak "kirajzoló hídként" szolgál: a CPU-s eredményt
    //    minden lépés után feltöltjük rá (lásd lejjebb, uploadVisualizationData).
    GPUBFS gpuBfs;
    gpuBfs.setOpenCL(clManager);
    gpuBfs.initialize(maze);

    // -----------------------------------------------------------------------
    // CPU mód: a mérés és a vizualizáció szét van választva.
    //
    // 1. MÉRÉS: a CPU BFS-t vizualizációtól függetlenül, tight loop-ban
    //    futtatjuk le teljesen – így az animációs késleltetés (bfsStepInterval)
    //    nem torzítja az időt.
    //
    // 2. VIZUALIZÁCIÓ: egy második BFS példány lépésről lépésre halad,
    //    amelyet az animációs ciklus vezérel.
    // -----------------------------------------------------------------------
    Profiler cpuProfiler;
    BFS      cpuBfsMeasure;   // csak méréshez
    BFS      cpuBfsVisual;    // csak vizualizációhoz

    if (mode == BFSMode::CPU)
    {
        std::cout << "CPU BFS merese (tight loop, vizualizacio nelkul)...\n";

        cpuBfsMeasure.initialize(maze);

        Stopwatch sw;
        sw.start();
        while (!cpuBfsMeasure.finished())
        {
            cpuBfsMeasure.step(maze);
            cpuProfiler.cpuSteps++;
        }
        sw.stop(cpuProfiler.cpuTotalMs);

        cpuProfiler.printCPU();
        std::cout << "(Vizualizacio indul...)\n";

        // Vizuális példány inicializálása – ez fog lépésről lépésre haladni
        cpuBfsVisual.initialize(maze);
    }

    // -----------------------------------------------------------------------
    // GPU mód: a mérés és a vizualizáció szét van választva.
    //
    // 1. MÉRÉS: egy külön GPUBFS példány tight loop-ban fut le teljesen –
    //    az animációs késleltetés (bfsStepInterval) nem torzítja az időt.
    //
    // 2. VIZUALIZÁCIÓ: a fenti gpuBfs objektum lépésről lépésre halad,
    //    amelyet az animációs ciklus vezérel. Ennek ideje NEM kerül mérésbe.
    // -----------------------------------------------------------------------
    Profiler gpuProfilerMeasure;

    if (mode == BFSMode::GPU)
    {
        std::cout << "GPU BFS merese (tight loop, vizualizacio nelkul)...\n";

        GPUBFS gpuBfsMeasure;
        gpuBfsMeasure.setOpenCL(clManager);
        gpuBfsMeasure.initialize(maze);

        while (!gpuBfsMeasure.finished())
            gpuBfsMeasure.step(maze);

        gpuProfilerMeasure = gpuBfsMeasure.getProfiler();
        gpuProfilerMeasure.printGPU();
        std::cout << "(Vizualizacio indul...)\n";

        // A vizualizációs gpuBfs már inicializálva van (5. lépésben),
        // az animációs ciklus ezt lépteti – nem mérjük
    }

    // GPU módban a vizualizációs gpuBfs-t léptetjük (nem mért),
    // CPU módban a cpuBfsVisual-t.
    BFSBase* bfsAlgo = (mode == BFSMode::GPU)
        ? static_cast<BFSBase*>(&gpuBfs)
        : static_cast<BFSBase*>(&cpuBfsVisual);

    const int    CPU_STEPS_PER_FRAME = 15;    // vizuális sebesség hangolása
    const double bfsStepInterval = 0.005;
    const double pathStepInterval = 0.005;

    double lastBfsStep = glfwGetTime();
    double lastPathStep = glfwGetTime();
    int    pathReveal = 0;
    bool   statsPrinted = false;

    while (!renderer.shouldClose())
    {
        double now = glfwGetTime();

        // BFS lépés – mindig a kiválasztott algoritmuson (CPU vagy GPU) keresztül.
        // Ez csak a VIZUALIZÁCIÓT vezérli, az időt NEM mérjük.
        if (!bfsAlgo->finished() && now - lastBfsStep >= bfsStepInterval) {
            lastBfsStep = now;

            if (mode == BFSMode::CPU)
            {
                // Vizuális példány léptetése – NEM mérjük, csak animálunk
                for (int i = 0; i < CPU_STEPS_PER_FRAME && !bfsAlgo->finished(); i++)
                    bfsAlgo->step(maze);

                gpuBfs.uploadVisualizationData(bfsAlgo->getDistances(),
                    bfsAlgo->getPath());
            }
            else {
                // Vizualizációs GPU példány léptetése – NEM mérjük
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
                    // A már korábban lefuttatott tight-loop mérés eredményét
                    // írjuk ki – NEM a vizualizációs példányét
                    const Profiler& gp = gpuProfilerMeasure;

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