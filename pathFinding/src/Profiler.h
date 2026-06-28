#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>

// Egyszerű wall-clock stopwatch
struct Stopwatch
{
    using Clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<Clock> t0;

    void start() { t0 = Clock::now(); }

    // Megállítja és hozzáadja az eltelt időt ms-ban a célváltozóhoz
    void stop(double& accumulatorMs)
    {
        auto dt = Clock::now() - t0;
        accumulatorMs += std::chrono::duration<double, std::milli>(dt).count();
    }
};

// Az összes mért adat egy helyen
struct Profiler
{
    // GPU BFS mérések (ms)
    double gpuKernelMs = 0.0;   // csak a bfs_step kernel futása
    double gpuH2DMs = 0.0;   // host→device transzferek (nextSize nullázás)
    double gpuD2HMs = 0.0;   // device→host transzferek (goalDist, nextSize, végső read)
    double gpuTotalMs = 0.0;   // teljes GPU BFS (minden step összege)

    // CPU BFS mérések (ms)
    double cpuTotalMs = 0.0;   // teljes CPU BFS (minden step összege)

    int    gpuSteps = 0;     // hány BFS iteráció futott le
    int    cpuSteps = 0;

    void printGPU() const
    {
        std::cout << "\n========== GPU BFS Profilozas ==========\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Kernel ido (bfs_step):     " << std::setw(10) << gpuKernelMs << " ms\n";
        std::cout << "  Host -> Device transzfer:  " << std::setw(10) << gpuH2DMs << " ms\n";
        std::cout << "  Device -> Host transzfer:  " << std::setw(10) << gpuD2HMs << " ms\n";
        std::cout << "  GPU BFS osszesen:          " << std::setw(10) << gpuTotalMs << " ms\n";
        std::cout << "  BFS iteraciok szama:       " << std::setw(10) << gpuSteps << "\n";
        if (gpuSteps > 0)
            std::cout << "  Atlag / iteracio:          " << std::setw(10)
            << gpuTotalMs / gpuSteps << " ms\n";
    }

    void printCPU() const
    {
        std::cout << "\n========== CPU BFS Profilozas ==========\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  CPU BFS osszesen:          " << std::setw(10) << cpuTotalMs << " ms\n";
        std::cout << "  BFS iteraciok szama:       " << std::setw(10) << cpuSteps << "\n";
        if (cpuSteps > 0)
            std::cout << "  Atlag / iteracio:          " << std::setw(10)
            << cpuTotalMs / cpuSteps << " ms\n";
    }

    void printSpeedup() const
    {
        if (cpuTotalMs <= 0.0 || gpuTotalMs <= 0.0) return;
        std::cout << "\n========== Speedup ==========\n";
        std::cout << std::fixed << std::setprecision(2);
        double speedup = cpuTotalMs / gpuTotalMs;
        std::cout << "  CPU ido / GPU ido = "
            << cpuTotalMs << " ms / " << gpuTotalMs << " ms"
            << " = " << speedup << "x\n";
        if (speedup >= 1.0)
            std::cout << "  → GPU " << speedup << "x gyorsabb\n";
        else
            std::cout << "  → CPU " << (1.0 / speedup) << "x gyorsabb "
            << "(kis meretu labirintusnal ez normalis)\n";
    }
};