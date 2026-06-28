#include "GPUBFS.h"
#include <fstream>
#include <iostream>

// -----------------------------------------------------------------------
// loadFile
//   Egyszerű segédfüggvény: beolvassa a megadott elérési úton lévő
//   szöveges fájlt (jelen esetben egy .cl kernel-forrást) teljes
//   egészében egy std::string-be. Ha a fájl nem létezik/nem olvasható,
//   üres stringet ad vissza (ezt az initialize() is ellenőrzi).
// -----------------------------------------------------------------------
static std::string loadFile(const char* path)
{
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>());
}

// -----------------------------------------------------------------------
GPUBFS::~GPUBFS() { releaseClResources(); }

// -----------------------------------------------------------------------
// releaseClResources
//   Minden korábban lefoglalt OpenCL objektumot (kernel, program, buffer)
//   felszabadít, és nullptr-re állítja a tagváltozókat. Mind a destruktor,
//   mind az initialize() (újragenerálás esetén) meghívja, hogy elkerüljük
//   az erőforrás-szivárgást.
// -----------------------------------------------------------------------
void GPUBFS::releaseClResources()
{
    if (bfsKernel) { clReleaseKernel(bfsKernel);        bfsKernel = nullptr; }
    if (bfsProgram) { clReleaseProgram(bfsProgram);      bfsProgram = nullptr; }
    if (colorKernel) { clReleaseKernel(colorKernel);      colorKernel = nullptr; }
    if (colorProgram) { clReleaseProgram(colorProgram);    colorProgram = nullptr; }
    if (mazeBuf) { clReleaseMemObject(mazeBuf);         mazeBuf = nullptr; }
    if (distBuf) { clReleaseMemObject(distBuf);         distBuf = nullptr; }
    if (parentBuf) { clReleaseMemObject(parentBuf);       parentBuf = nullptr; }
    if (frontierBuf) { clReleaseMemObject(frontierBuf);     frontierBuf = nullptr; }
    if (frontierSizeBuf) { clReleaseMemObject(frontierSizeBuf); frontierSizeBuf = nullptr; }
    if (nextFrontierBuf) { clReleaseMemObject(nextFrontierBuf); nextFrontierBuf = nullptr; }
    if (nextSizeBuf) { clReleaseMemObject(nextSizeBuf);     nextSizeBuf = nullptr; }
    if (pathMaskBuf) { clReleaseMemObject(pathMaskBuf);     pathMaskBuf = nullptr; }
}

// Beállítja, melyik OpenCLManager-t (context/device/queue) használja a kernelfutáshoz.
void GPUBFS::setOpenCL(OpenCLManager& manager) { clm = &manager; }

// -----------------------------------------------------------------------
// buildProgram
//   Lefordít egy OpenCL programot a megadott forrásszövegből, kiírja a
//   build-logot (ha van rajta üzenet, pl. figyelmeztetés vagy hiba),
//   majd létrehozza a megadott nevű kernelt a lefordított programból.
//   Hiba esetén false-szal tér vissza, és konzolra írja a hibakódot.
// -----------------------------------------------------------------------
bool GPUBFS::buildProgram(const std::string& src, const std::string& label,
    cl_program& outProg, cl_kernel& outKernel,
    const char* kernelName)
{
    const char* csrc = src.c_str();
    cl_int err = CL_SUCCESS;

    // Program létrehozása a forrásszövegből
    outProg = clCreateProgramWithSource(clm->getContext(), 1, &csrc, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << label << ": clCreateProgramWithSource failed: " << err << "\n";
        return false;
    }

    // Fordítás az aktuális eszközre
    err = clBuildProgram(outProg, 0, nullptr, nullptr, nullptr, nullptr);

    // Build log kiolvasása és kiírása (akkor is hasznos, ha sikeres volt
    // a fordítás, mert figyelmeztetéseket is tartalmazhat)
    size_t logSize = 0;
    clGetProgramBuildInfo(outProg, clm->getDevice(), CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
    if (logSize > 1) {
        std::vector<char> log(logSize + 1, 0);
        clGetProgramBuildInfo(outProg, clm->getDevice(), CL_PROGRAM_BUILD_LOG,
            logSize, log.data(), nullptr);
        std::cout << label << " build log:\n" << log.data() << "\n";
    }
    if (err != CL_SUCCESS) {
        std::cout << label << ": clBuildProgram failed: " << err << "\n";
        return false;
    }

    // A kernel "kimetszése" a lefordított programból, név alapján
    outKernel = clCreateKernel(outProg, kernelName, &err);
    if (err != CL_SUCCESS) {
        std::cout << label << ": clCreateKernel('" << kernelName << "') failed: " << err << "\n";
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------
// initialize
//   Felkészíti a GPU-s BFS-t egy új labirintusra:
//     1) felszabadítja az esetlegesen korábbi futásból megmaradt
//        OpenCL erőforrásokat,
//     2) megkeresi a START/GOAL cellákat a host oldalon,
//     3) létrehozza és feltölti az összes GPU buffert (maze, distance,
//        parent, frontier-listák, pathMask),
//     4) beolvassa és lefordítja a bfs.cl és color.cl kernel-forrásokat.
// -----------------------------------------------------------------------
void GPUBFS::initialize(const Maze& maze)
{
    releaseClResources();

    width = maze.getWidth();
    height = maze.getHeight();
    int size = width * height;

    // Host-oldali tükörmásolatok alaphelyzetbe állítása
    hostDist.assign(size, -1);
    hostParent.assign(size, -1);
    hostPath.clear();
    hostPathLen = 0;

    startIndex = -1;
    goalIndex = -1;

    // START/GOAL cellák megkeresése a rácsban
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (maze.get(x, y) == START) startIndex = idx;
            if (maze.get(x, y) == GOAL)  goalIndex = idx;
        }

    // A start cella távolsága 0, ez lesz az egyetlen kezdő frontier-elem
    hostDist[startIndex] = 0;
    currentDist = 0;
    done = false;
    foundFlag = false;
    hostFrontierSize = 1;

    // --- GPU bufferek ---
    // mazeBuf: csak olvasott, a labirintus celláinak típusait tartalmazza
    mazeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        size * sizeof(uint8_t), (void*)maze.data().data(), nullptr);

    // distBuf: BFS-távolságok, kezdetben minden cella -1 (kivéve a startot)
    distBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostDist.data(), nullptr);

    // parentBuf: melyik cellából értük el adott cellát (útvonal-rekonstrukció)
    parentBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostParent.data(), nullptr);

    // compact frontier indexlista – kezdetben csak a start cella van benne
    std::vector<int> initFrontier(size, 0);
    initFrontier[0] = startIndex;
    frontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), initFrontier.data(), nullptr);

    int initSize = 1;
    frontierSizeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        sizeof(int), &initSize, nullptr);

    // a következő körben felfedezett cellák listája – kezdetben üres
    std::vector<int> emptyList(size, 0);
    nextFrontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), emptyList.data(), nullptr);

    // nextSizeBuf: atomikus számláló, hány elemet írtak a nextFrontierBuf-ba
    int zero = 0;
    nextSizeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        sizeof(int), &zero, nullptr);

    // pathMaskBuf: -1-ekkel inicializálva (senki sem része még az útvonalnak)
    std::vector<int> minusOnes(size, -1);
    pathMaskBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), minusOnes.data(), nullptr);

    // --- Kernelek betöltése ---
    // KERNEL_DIR egy build-időben (CMake) definiált makró, amely a .cl
    // fájlok elérési útját adja meg; ha nincs definiálva, alapértelmezésként
    // a futtatható melletti "kernels/" mappát próbáljuk.
#ifdef KERNEL_DIR
    std::string kernelDir = std::string(KERNEL_DIR);
#else
    std::string kernelDir = "kernels/";
#endif

    std::string bfsSrc = loadFile((kernelDir + "bfs.cl").c_str());
    std::string colorSrc = loadFile((kernelDir + "color.cl").c_str());

    if (bfsSrc.empty()) {
        std::cout << "HIBA: nem sikerult beolvasni: " << kernelDir << "bfs.cl\n";
        return;
    }
    if (colorSrc.empty()) {
        std::cout << "HIBA: nem sikerult beolvasni: " << kernelDir << "color.cl\n";
        return;
    }

    // A két kernel (BFS-lépés és vizualizáció) lefordítása
    buildProgram(bfsSrc, "BFS kernel", bfsProgram, bfsKernel, "bfs_step");
    buildProgram(colorSrc, "Color kernel", colorProgram, colorKernel, "color_maze");
}

// -----------------------------------------------------------------------
// step
//   Egy BFS "hullámfront-kiterjesztés" elindítása a GPU-n:
//     1) lenullázzuk a next frontier méretét számláló buffert,
//     2) beállítjuk a kernel argumentumait és elindítjuk 1D NDRange-ben,
//        annyi work-itemmel, ahány cella jelenleg a frontier-ben van,
//     3) megvárjuk a kernel végét (clFinish),
//     4) minimális adatot olvasunk vissza: csak a goal cella távolságát
//        és az új frontier méretét (nem az egész tömböket – ez sokkal
//        gyorsabb nagy labirintusok esetén),
//     5) ha a goal elérhetővé vált, visszaolvassuk a teljes parent/dist
//        tömböt is, és rekonstruáljuk az utat.
// -----------------------------------------------------------------------
bool GPUBFS::step(const Maze&)
{
    // Ha már korábban lezárult az algoritmus, nincs több dolgunk
    if (done) return true;

    // Ha a kernel nem épült fel sikeresen (pl. hiányzó .cl fájl), nem
    // tudunk lépni – jelezzük a hibát és zárjuk le sikertelenként.
    if (!bfsKernel) {
        std::cout << "GPUBFS::step() – nincs érvényes BFS kernel\n";
        done = true; foundFlag = false; return true;
    }

    Stopwatch swTotal, swH2D, swKernel, swD2H;
    swTotal.start();

    // nextSize nullázása – minden körben újra kell kezdeni a számlálást
    swH2D.start();
    int zero = 0;
    clEnqueueWriteBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &zero, 0, nullptr, nullptr);

    swH2D.stop(profiler.gpuD2HMs);

    // Kernel args
    // bfs_step(maze, distance, parent, frontierList, frontierSize,
    //          nextFrontierList, nextSize, width, height, currentDist)
    swKernel.start();
    clSetKernelArg(bfsKernel, 0, sizeof(cl_mem), &mazeBuf);
    clSetKernelArg(bfsKernel, 1, sizeof(cl_mem), &distBuf);
    clSetKernelArg(bfsKernel, 2, sizeof(cl_mem), &parentBuf);
    clSetKernelArg(bfsKernel, 3, sizeof(cl_mem), &frontierBuf);
    clSetKernelArg(bfsKernel, 4, sizeof(int), &hostFrontierSize);
    clSetKernelArg(bfsKernel, 5, sizeof(cl_mem), &nextFrontierBuf);
    clSetKernelArg(bfsKernel, 6, sizeof(cl_mem), &nextSizeBuf);
    clSetKernelArg(bfsKernel, 7, sizeof(int), &width);
    clSetKernelArg(bfsKernel, 8, sizeof(int), &height);
    clSetKernelArg(bfsKernel, 9, sizeof(int), &currentDist);

    // Egy work-item / frontier-cella: pontosan annyi szálat indítunk,
    // ahány eleme van a jelenlegi hullámfrontnak.
    size_t global = static_cast<size_t>(hostFrontierSize);
    clEnqueueNDRangeKernel(clm->getQueue(), bfsKernel, 1,
        nullptr, &global, nullptr, 0, nullptr, nullptr);
    clFinish(clm->getQueue());
    swKernel.stop(profiler.gpuKernelMs);

    // --- Optimalizált visszaolvasás ---
    // Csak 1 int-et olvasunk vissza a distBuf-ból (a goal cellát),
    // nem az egész tömböt (1000x1000 esetén ez 4MB vs 4 byte).
    // A teljes distBuf és parentBuf visszaolvasása csak akkor történik,
    // ha a goal elérhetővé vált.
    swD2H.start();
    int goalDist = -1;
    clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE,
        goalIndex * sizeof(int),   // offset: csak a goal cellája
        sizeof(int), &goalDist,
        0, nullptr, nullptr);

    // Az új (következő körben feldolgozandó) frontier mérete
    int hostNextSize = 0;
    clEnqueueReadBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &hostNextSize, 0, nullptr, nullptr);
    swD2H.stop(profiler.gpuD2HMs);

    // A "next frontier" lesz a következő kör frontier-je (ping-pong buffercsere)
    std::swap(frontierBuf, nextFrontierBuf);
    hostFrontierSize = hostNextSize;
    currentDist++;
    profiler.gpuSteps++;

    if (goalDist != -1)
    {
        // Goal elérve: most olvassuk vissza a teljes parentBuf-ot
        // az útvonal rekonstruálásához (ez csak egyszer fut le)
        int size = width * height;
        swD2H.start();
        clEnqueueReadBuffer(clm->getQueue(), parentBuf, CL_TRUE, 0,
            sizeof(int) * size, hostParent.data(), 0, nullptr, nullptr);

        // distBuf-ot is visszaolvassuk a vizualizációhoz (color kernel
        // GPU-n olvassa, de hostDist kell a getDistances()-hez)
        clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE, 0,
            sizeof(int) * size, hostDist.data(), 0, nullptr, nullptr);
        swD2H.stop(profiler.gpuD2HMs);
        reconstructPath(goalIndex);

        // pathMask feltöltése GPU-ra:
        // hostPath[0] = goal, hostPath[last] = start
        // A start felőli animációhoz: path[pathLen-1-i] az i-edik megjelenített cella
        // Tehát pathMask[path[pathLen-1-i]] = i
        swH2D.start();
        std::vector<int> hostPathMask(width * height, -1);
        for (int i = 0; i < hostPathLen; i++)
            hostPathMask[hostPath[hostPathLen - 1 - i]] = i;

        clEnqueueWriteBuffer(clm->getQueue(), pathMaskBuf, CL_TRUE, 0,
            sizeof(int) * width * height,
            hostPathMask.data(), 0, nullptr, nullptr);
        swH2D.stop(profiler.gpuH2DMs);
        foundFlag = true;
        done = true;
        swTotal.stop(profiler.gpuTotalMs);
        return true;
    }

    // Nincs több felfedezhető cella (a frontier kiürült), vagy elméletileg
    // lehetetlenül sok kör telt el már (biztonsági korlát) -> nincs út a célhoz.
    if (hostNextSize == 0 || currentDist > width * height) {
        foundFlag = false;
        done = true;
    }

    swTotal.stop(profiler.gpuTotalMs);
    return done;
}

// -----------------------------------------------------------------------
// reconstructPath
//   A (GPU-ról frissen visszaolvasott) hostParent tömb mentén visszafelé
//   lépkedve felépíti a hostPath listát a goal cellától a start celláig
//   (amelynek parent értéke -1).
// -----------------------------------------------------------------------
void GPUBFS::reconstructPath(int goal)
{
    hostPath.clear();
    int cur = goal;
    while (cur != -1) {
        hostPath.push_back(cur);
        cur = hostParent[cur];
    }
    hostPathLen = (int)hostPath.size();
}

// -----------------------------------------------------------------------
// updateColorTexture
//   Lefuttatja a color_maze kernelt, amely közvetlenül a GL textúrába ír.
//   Nincs CPU roundtrip: distBuf és pathMaskBuf GPU-n marad,
//   colorImage pedig a GL-lel megosztott memória.
// -----------------------------------------------------------------------
void GPUBFS::updateColorTexture(cl_mem colorImage, int revealCount)
{
    if (!colorKernel || !colorImage) return;

    // 1) GL-t értesítjük: az OpenCL átveszi a textúra ownership-jét
    clEnqueueAcquireGLObjects(clm->getQueue(), 1, &colorImage, 0, nullptr, nullptr);

    // 2) Color kernel futtatása
    // color_maze(maze, distance, pathMask, revealCount, colorOut, width, height)
    clSetKernelArg(colorKernel, 0, sizeof(cl_mem), &mazeBuf);
    clSetKernelArg(colorKernel, 1, sizeof(cl_mem), &distBuf);
    clSetKernelArg(colorKernel, 2, sizeof(cl_mem), &pathMaskBuf); // O(1) lookup
    clSetKernelArg(colorKernel, 3, sizeof(int), &revealCount);
    clSetKernelArg(colorKernel, 4, sizeof(cl_mem), &colorImage);
    clSetKernelArg(colorKernel, 5, sizeof(int), &width);
    clSetKernelArg(colorKernel, 6, sizeof(int), &height);

    // 2D NDRange: egy work-item / cella, a teljes rácsra kiterjesztve
    size_t global[2] = { (size_t)width, (size_t)height };
    clEnqueueNDRangeKernel(clm->getQueue(), colorKernel, 2,
        nullptr, global, nullptr, 0, nullptr, nullptr);

    // 3) GL visszakapja a textúrát – szinkronizáció
    clEnqueueReleaseGLObjects(clm->getQueue(), 1, &colorImage, 0, nullptr, nullptr);
    clFinish(clm->getQueue());
}

// -----------------------------------------------------------------------
// uploadVisualizationData
//   "Híd" funkció: lehetővé teszi, hogy egy a GPUBFS-től független
//   (pl. CPU-s, std::queue-alapú BFS::) algoritmus eredményét is meg
//   tudjuk jeleníteni ugyanazon a GPU-s color_maze kernelen keresztül.
//
//   A főciklus (main.cpp) CPU módban minden BFS-lépés után ezt hívja meg
//   a CPU-algoritmus aktuális distances/path adataival – ettől
//   függetlenül, hogy ezeket nem a GPU számolta ki, a distBuf és a
//   pathMaskBuf pontosan ugyanúgy néz ki utána, mintha a bfs_step kernel
//   futott volna, így a vizualizáció (updateColorTexture) változatlanul
//   működik.
// -----------------------------------------------------------------------
void GPUBFS::uploadVisualizationData(const std::vector<int>& distances, const std::vector<int>& path)
{
    // Csak akkor van értelme, ha initialize() már lefoglalta a buffereket
    if (!distBuf || !pathMaskBuf) return;

    int size = width * height;

    // Távolság-tömb feltöltése – a color kernel ez alapján színezi ki
    // a már bejárt ("visited") cellákat.
    clEnqueueWriteBuffer(clm->getQueue(), distBuf, CL_TRUE, 0,
        sizeof(int) * size, distances.data(), 0, nullptr, nullptr);

    // pathMask felépítése ugyanúgy, mint a step()-ben:
    // path[0] = goal, path[last] = start, az animáció a start felől indul,
    // ezért pathMask[path[pathLen-1-i]] = i.
    std::vector<int> mask(size, -1);
    int pathLen = (int)path.size();
    for (int i = 0; i < pathLen; i++)
        mask[path[pathLen - 1 - i]] = i;

    clEnqueueWriteBuffer(clm->getQueue(), pathMaskBuf, CL_TRUE, 0,
        sizeof(int) * size, mask.data(), 0, nullptr, nullptr);

    // Host-oldali tükörmásolatok frissítése, hogy a gpuBfs.getDistances()/
    // getPath() is konzisztens adatot adjon vissza, ha valaki közvetlenül
    // ezt az objektumot kérdezné le.
    hostDist = distances;
    hostPath = path;
    hostPathLen = pathLen;
}