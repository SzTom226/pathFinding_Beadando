#pragma once

#include "BFSBase.h"
#include "Maze.h"
#include "OpenCLManager.h"
#include "Profiler.h"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <vector>
#include <string>

// ============================================================================
// GPUBFS
//
// A BFSBase interfész GPU-gyorsított implementációja OpenCL-lel.
// A BFS "hullámfront" (frontier) kiterjesztését egy OpenCL kernel
// (bfs_step, lásd bfs.cl) végzi párhuzamosan: minden work-item egy
// frontier-beli cellát dolgoz fel, és a frissen felfedezett szomszédokat
// atomikus műveletekkel (CAS, atomic_inc) írja be egy kompakt
// "next frontier" listába – így nincs szükség zárolásra a work-item-ek
// között.
//
// A vizualizációhoz egy másik kernel (color_maze, lásd color.cl) közvetlenül
// egy OpenGL textúrába ír OpenCL/OpenGL interop segítségével (lásd
// updateColorTexture), elkerülve a felesleges CPU-GPU adatmozgást.
// ============================================================================
class GPUBFS : public BFSBase
{
public:
    ~GPUBFS() override;

    // Beállítja, melyik OpenCL contextust/queue-t/device-ot használja
    // a kernelek futtatásához és a GL interophoz.
    void setOpenCL(OpenCLManager& manager);

    // Lefoglalja/feltölti a GPU buffereket a labirintusból, majd betölti
    // és felépíti (lefordítja) a bfs.cl és color.cl kernel-programokat.
    void initialize(const Maze& maze) override;

    // Egy BFS-kör (hullámfront-kiterjesztés) elindítása a GPU-n,
    // majd a szükséges minimális mennyiségű adat (csak a goal cella
    // távolsága + a következő frontier mérete) visszaolvasása.
    bool step(const Maze& maze) override;

    bool finished()   const override { return done; }
    bool pathFound()  const override { return foundFlag; }
    const std::vector<int>& getDistances() const override { return hostDist; }
    const std::vector<int>& getPath()      const override { return hostPath; }

    // Interop: a color kernel futtatása a megosztott GL textúrára
    // colorImage = Renderer::getColorImage()
    void updateColorTexture(cl_mem colorImage, int revealCount);

    // Lehetővé teszi, hogy egy MÁSIK (pl. CPU-s) BFS implementáció
    // eredményét is meg lehessen jeleníteni ugyanazon a GPU-s
    // colorMaze pipeline-on keresztül: feltölti a distBuf-ot és a
    // pathMaskBuf-ot a megadott adatokkal, anélkül, hogy a GPU BFS
    // kernel (bfs_step) futna. Ezt kell hívni minden CPU-s BFS lépés
    // után, ha a kirajzolást ez a GPUBFS objektum végzi.
    void uploadVisualizationData(const std::vector<int>& distances, const std::vector<int>& path);

    //Profilizási adatok lekérése
    const Profiler& getProfiler() const { return profiler; }

private:
    // A hostParent tömb alapján (amit csak a cél elérésekor olvasunk
    // vissza a GPU-ról) felépíti a hostPath listát GOAL -> START sorrendben.
    void reconstructPath(int goal);

    // Felszabadítja az összes korábban lefoglalt OpenCL erőforrást
    // (kernelek, programok, bufferek) – újrainicializáláskor és a
    // destruktorban is hívva, hogy ne legyen memóriaszivárgás.
    void releaseClResources();

    // Segédfüggvény: forrásszövegből lefordít egy OpenCL programot,
    // kiírja a build logot (ha van), és létrehozza a megadott nevű kernelt.
    bool buildProgram(const std::string& src, const std::string& label,
        cl_program& outProg, cl_kernel& outKernel,
        const char* kernelName);

private:
    OpenCLManager* clm = nullptr;   // a használt OpenCL context/queue/device

    int width = 0, height = 0;
    int currentDist = 0;     // az éppen feldolgozott hullámfront "rétege" (BFS-mélység)
    int startIndex = -1;
    int goalIndex = -1;
    bool done = false;       // az algoritmus lefutott (siker vagy kudarc)
    bool foundFlag = false;  // sikerült elérni a GOAL-t

    // BFS kernel (bfs.cl: bfs_step) – egy hullámfront-lépés végrehajtása
    cl_program bfsProgram = nullptr;
    cl_kernel  bfsKernel = nullptr;

    // Color kernel (color.cl: color_maze) – a vizualizációs textúra feltöltése
    cl_program colorProgram = nullptr;
    cl_kernel  colorKernel = nullptr;

    // --- BFS bufferek (mind a GPU oldalon élnek) ---
    cl_mem mazeBuf = nullptr;          // a labirintus celláinak típusa (WALL/EMPTY/START/GOAL)
    cl_mem distBuf = nullptr;          // BFS-távolságok cellánként (-1 = nem látogatott)
    cl_mem parentBuf = nullptr;        // útvonal-rekonstrukcióhoz: melyik cellából értük el
    cl_mem frontierBuf = nullptr;      // jelenlegi hullámfront (kompakt indexlista)
    cl_mem frontierSizeBuf = nullptr;  // (a hostFrontierSize tartja számon a méretet a host oldalon)
    cl_mem nextFrontierBuf = nullptr;  // a következő körben feldolgozandó cellák listája
    cl_mem nextSizeBuf = nullptr;      // atomikus számláló: hány elem van a next frontier-ben

    // pathMask: pathMask[idx] = animáció-sorszám, ha az idx rajta van az útvonalon,
    // különben -1. A color kernel O(1)-ben nézi fel, nincs szükség ciklusra.
    cl_mem pathMaskBuf = nullptr;
    int    hostPathLen = 0;

    // --- Host-oldali tükörmásolatok (csak akkor frissülnek, ha szükséges) ---
    std::vector<int> hostDist;     // getDistances() ezt adja vissza
    std::vector<int> hostParent;   // csak a cél elérésekor olvassuk vissza a GPU-ról
    std::vector<int> hostPath;     // a végső, rekonstruált útvonal

    int hostFrontierSize = 0;   // hány elem van jelenleg a frontierBuf-ban

    Profiler profiler;
};