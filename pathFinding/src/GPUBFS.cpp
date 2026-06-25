#include "GPUBFS.h"
#include <fstream>
#include <iostream>

static std::string loadFile(const char* path)
{
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>());
}

// -----------------------------------------------------------------------
GPUBFS::~GPUBFS() { releaseClResources(); }

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

void GPUBFS::setOpenCL(OpenCLManager& manager) { clm = &manager; }

// -----------------------------------------------------------------------
bool GPUBFS::buildProgram(const std::string& src, const std::string& label,
    cl_program& outProg, cl_kernel& outKernel,
    const char* kernelName)
{
    const char* csrc = src.c_str();
    cl_int err = CL_SUCCESS;

    outProg = clCreateProgramWithSource(clm->getContext(), 1, &csrc, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << label << ": clCreateProgramWithSource failed: " << err << "\n";
        return false;
    }

    err = clBuildProgram(outProg, 0, nullptr, nullptr, nullptr, nullptr);

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

    outKernel = clCreateKernel(outProg, kernelName, &err);
    if (err != CL_SUCCESS) {
        std::cout << label << ": clCreateKernel('" << kernelName << "') failed: " << err << "\n";
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------
void GPUBFS::initialize(const Maze& maze)
{
    releaseClResources();

    width = maze.getWidth();
    height = maze.getHeight();
    int size = width * height;

    hostDist.assign(size, -1);
    hostParent.assign(size, -1);
    hostPath.clear();
    hostPathLen = 0;

    startIndex = -1;
    goalIndex = -1;

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (maze.get(x, y) == START) startIndex = idx;
            if (maze.get(x, y) == GOAL)  goalIndex = idx;
        }

    hostDist[startIndex] = 0;
    currentDist = 0;
    done = false;
    foundFlag = false;
    hostFrontierSize = 1;

    // --- GPU bufferek ---
    mazeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        size * sizeof(uint8_t), (void*)maze.data().data(), nullptr);

    distBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostDist.data(), nullptr);

    parentBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostParent.data(), nullptr);

    // compact frontier indexlista
    std::vector<int> initFrontier(size, 0);
    initFrontier[0] = startIndex;
    frontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), initFrontier.data(), nullptr);

    int initSize = 1;
    frontierSizeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        sizeof(int), &initSize, nullptr);

    std::vector<int> emptyList(size, 0);
    nextFrontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), emptyList.data(), nullptr);

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

    buildProgram(bfsSrc, "BFS kernel", bfsProgram, bfsKernel, "bfs_step");
    buildProgram(colorSrc, "Color kernel", colorProgram, colorKernel, "color_maze");
}

// -----------------------------------------------------------------------
bool GPUBFS::step(const Maze&)
{
    if (done) return true;

    if (!bfsKernel) {
        std::cout << "GPUBFS::step() – nincs érvényes BFS kernel\n";
        done = true; foundFlag = false; return true;
    }

    // nextSize nullázása
    int zero = 0;
    clEnqueueWriteBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &zero, 0, nullptr, nullptr);

    // Kernel args
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

    size_t global = static_cast<size_t>(hostFrontierSize);
    clEnqueueNDRangeKernel(clm->getQueue(), bfsKernel, 1,
        nullptr, &global, nullptr, 0, nullptr, nullptr);
    clFinish(clm->getQueue());

    // --- Optimalizált visszaolvasás ---
    // Csak 1 int-et olvasunk vissza a distBuf-ból (a goal cellát),
    // nem az egész tömböt (1000x1000 esetén ez 4MB vs 4 byte).
    // A teljes distBuf és parentBuf visszaolvasása csak akkor történik,
    // ha a goal elérhetővé vált.
    int goalDist = -1;
    clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE,
        goalIndex * sizeof(int),   // offset: csak a goal cellája
        sizeof(int), &goalDist,
        0, nullptr, nullptr);

    int hostNextSize = 0;
    clEnqueueReadBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &hostNextSize, 0, nullptr, nullptr);

    std::swap(frontierBuf, nextFrontierBuf);
    hostFrontierSize = hostNextSize;
    currentDist++;

    if (goalDist != -1)
    {
        // Goal elérve: most olvassuk vissza a teljes parentBuf-ot
        // az útvonal rekonstruálásához (ez csak egyszer fut le)
        int size = width * height;
        clEnqueueReadBuffer(clm->getQueue(), parentBuf, CL_TRUE, 0,
            sizeof(int) * size, hostParent.data(), 0, nullptr, nullptr);

        // distBuf-ot is visszaolvassuk a vizualizációhoz (color kernel
        // GPU-n olvassa, de hostDist kell a getDistances()-hez)
        clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE, 0,
            sizeof(int) * size, hostDist.data(), 0, nullptr, nullptr);

        reconstructPath(goalIndex);

        // pathMask feltöltése GPU-ra:
        // hostPath[0] = goal, hostPath[last] = start
        // A start felőli animációhoz: path[pathLen-1-i] az i-edik megjelenített cella
        // Tehát pathMask[path[pathLen-1-i]] = i
        std::vector<int> hostPathMask(width * height, -1);
        for (int i = 0; i < hostPathLen; i++)
            hostPathMask[hostPath[hostPathLen - 1 - i]] = i;

        clEnqueueWriteBuffer(clm->getQueue(), pathMaskBuf, CL_TRUE, 0,
            sizeof(int) * width * height,
            hostPathMask.data(), 0, nullptr, nullptr);

        foundFlag = true;
        done = true;
        return true;
    }

    if (hostNextSize == 0 || currentDist > width * height) {
        foundFlag = false;
        done = true;
    }

    return done;
}

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

    size_t global[2] = { (size_t)width, (size_t)height };
    clEnqueueNDRangeKernel(clm->getQueue(), colorKernel, 2,
        nullptr, global, nullptr, 0, nullptr, nullptr);

    // 3) GL visszakapja a textúrát – szinkronizáció
    clEnqueueReleaseGLObjects(clm->getQueue(), 1, &colorImage, 0, nullptr, nullptr);
    clFinish(clm->getQueue());
}