#pragma once

#include "Maze.h"
#include "BFSBase.h"
#include <queue>

// ============================================================================
// BFS
//
// A BFSBase interfész egyszerű, egyszálú CPU-s implementációja.
// Klasszikus szélességi keresés (breadth-first search) egy FIFO sorral
// (std::queue), amely a labirintus rácsát egydimenziós tömbként kezeli
// (idx = y * width + x).
//
// Ez az implementáció szolgál referenciaként/összehasonlítási alapként
// a GPU-s (GPUBFS) változathoz: ugyanazt az eredményt kell adnia, csak
// lépésenként, a CPU-n, párhuzamosítás nélkül.
// ============================================================================
class BFS : public BFSBase
{
public:
    // Megkeresi a START/GOAL cellákat, és felveszi a startot a frontier-be.
    void initialize(const Maze& maze) override;

    // Kivesz egy cellát a frontier elejéről, és felveszi a még nem
    // látogatott szomszédjait (ez egyetlen BFS-lépés).
    bool step(const Maze& maze) override;

    bool finished() const override { return finishedFlag; }
    bool pathFound() const override { return foundFlag; }

    const std::vector<int>& getDistances() const override { return dist; }
    const std::vector<int>& getPath() const override { return path; }

private:
    // GOAL-tól visszafelé lépkedve (parent tömb) felépíti a path tömböt.
    void reconstructPath(int goal);

private:
    std::queue<int> frontier;   // még feldolgozandó cellák (FIFO sor)

    std::vector<int> dist;      // dist[idx]   = start-tól mért távolság, -1 = nem látogatott
    std::vector<int> parent;    // parent[idx] = az a cella, ahonnan elértük (útvonal-rekonstrukcióhoz)
    std::vector<int> path;      // a végső útvonal (GOAL -> START sorrendben)

    int width = 0;
    int height = 0;

    int startIndex = -1;   // START cella 1D indexe
    int goalIndex = -1;    // GOAL cella 1D indexe

    bool finishedFlag = false;  // az algoritmus lefutott (siker vagy kudarc)
    bool foundFlag = false;     // sikerült elérni a GOAL-t
};