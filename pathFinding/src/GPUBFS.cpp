#include "GPUBFS.h"
#include <fstream>
#include <iostream>

static std::string loadFile(const char* path)
{
    std::ifstream f(path);
    return std::string(
        std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>());
}

GPUBFS::~GPUBFS()
{
    releaseClResources();
}

void GPUBFS::releaseClResources()
{
    if (kernel) { clReleaseKernel(kernel);           kernel = nullptr; }
    if (program) { clReleaseProgram(program);         program = nullptr; }
    if (mazeBuf) { clReleaseMemObject(mazeBuf);       mazeBuf = nullptr; }
    if (distBuf) { clReleaseMemObject(distBuf);       distBuf = nullptr; }
    if (parentBuf) { clReleaseMemObject(parentBuf);     parentBuf = nullptr; }
    if (frontierBuf) { clReleaseMemObject(frontierBuf);   frontierBuf = nullptr; }
    if (frontierSizeBuf) { clReleaseMemObject(frontierSizeBuf); frontierSizeBuf = nullptr; }
    if (nextFrontierBuf) { clReleaseMemObject(nextFrontierBuf); nextFrontierBuf = nullptr; }
    if (nextSizeBuf) { clReleaseMemObject(nextSizeBuf);   nextSizeBuf = nullptr; }
}

void GPUBFS::setOpenCL(OpenCLManager& manager)
{
    clm = &manager;
}

void GPUBFS::initialize(const Maze& maze)
{
    releaseClResources();

    width = maze.getWidth();
    height = maze.getHeight();
    int size = width * height;

    hostDist.assign(size, -1);
    hostParent.assign(size, -1);
    hostPath.clear();

    startIndex = -1;
    goalIndex = -1;

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
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

    // frontierBuf: indexlista – első elem a startIndex
    // max méret = size (ha az egész rács frontier lenne)
    std::vector<int> initFrontier(size, 0);
    initFrontier[0] = startIndex;

    frontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), initFrontier.data(), nullptr);

    // frontierSizeBuf: 1 (csak a start)
    int initSize = 1;
    frontierSizeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        sizeof(int), &initSize, nullptr);

    // nextFrontierBuf: üres lista
    std::vector<int> emptyList(size, 0);
    nextFrontierBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), emptyList.data(), nullptr);

    // nextSizeBuf: 0 (atomic counter)
    int zero = 0;
    nextSizeBuf = clCreateBuffer(clm->getContext(),
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        sizeof(int), &zero, nullptr);

    // --- Kernel betöltés ---

#ifdef KERNEL_DIR
    std::string kernelPath = std::string(KERNEL_DIR) + "bfs.cl";
#else
    std::string kernelPath = "kernels/bfs.cl";
#endif

    std::string src = loadFile(kernelPath.c_str());

    if (src.empty())
    {
        std::cout << "HIBA: nem sikerult beolvasni a kernel forrasfajlt ('"
            << kernelPath << "'). Ellenorizd, hogy letezik-e a "
            << "'kernels/bfs.cl' fajl a projekt forrasfajanak gyokereben "
            << "(ott, ahol a CMakeLists.txt is van), es hogy a "
            << "CMakeLists.txt-ben be van allitva a KERNEL_DIR makro.\n";
        kernel = nullptr;
        program = nullptr;
        return;
    }

    const char* csrc = src.c_str();
    cl_int err = CL_SUCCESS;

    program = clCreateProgramWithSource(clm->getContext(), 1, &csrc, nullptr, &err);
    if (err != CL_SUCCESS || program == nullptr)
    {
        std::cout << "clCreateProgramWithSource hiba, error code: " << err << std::endl;
        kernel = nullptr;
        return;
    }

    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);

    size_t logSize = 0;
    clGetProgramBuildInfo(program, clm->getDevice(), CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
    if (logSize > 1)
    {
        std::vector<char> log(logSize + 1, 0);
        clGetProgramBuildInfo(program, clm->getDevice(), CL_PROGRAM_BUILD_LOG,
            logSize, log.data(), nullptr);
        std::cout << "OpenCL build log:\n" << log.data() << std::endl;
    }

    if (err != CL_SUCCESS)
    {
        std::cout << "clBuildProgram hiba, error code: " << err << std::endl;
        kernel = nullptr;
        return;
    }

    kernel = clCreateKernel(program, "bfs_step", &err);
    if (err != CL_SUCCESS || kernel == nullptr)
    {
        std::cout << "Failed to create kernel 'bfs_step', error code: " << err << std::endl;
        kernel = nullptr;
    }
}

bool GPUBFS::step(const Maze&)
{
    if (done) return true;

    if (!kernel)
    {
        std::cout << "GPUBFS::step() - nincs ervenyes kernel, a BFS nem tud futni.\n";
        done = true;
        foundFlag = false;
        return true;
    }

    // --- nextFrontier és nextSize nullázása ---
    // Csak a frontierBuf-nyi részt kell törölni, nem az egész size-t
    std::vector<int> zeros(hostFrontierSize, 0);

    int zero = 0;
    clEnqueueWriteBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &zero, 0, nullptr, nullptr);

    // --- Kernel argumentumok ---
    // bfs_step(maze, dist, parent, frontierList, frontierSize,
    //          nextFrontierList, nextSize, width, height, currentDist)
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &mazeBuf);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &distBuf);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &parentBuf);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &frontierBuf);
    clSetKernelArg(kernel, 4, sizeof(int), &hostFrontierSize);
    clSetKernelArg(kernel, 5, sizeof(cl_mem), &nextFrontierBuf);
    clSetKernelArg(kernel, 6, sizeof(cl_mem), &nextSizeBuf);
    clSetKernelArg(kernel, 7, sizeof(int), &width);
    clSetKernelArg(kernel, 8, sizeof(int), &height);
    clSetKernelArg(kernel, 9, sizeof(int), &currentDist);

    // global size = pontosan annyi work-item, ahány frontier-cella van
    size_t global = static_cast<size_t>(hostFrontierSize);

    clEnqueueNDRangeKernel(clm->getQueue(), kernel, 1,
        nullptr, &global, nullptr, 0, nullptr, nullptr);
    clFinish(clm->getQueue());

    // --- Eredmények visszaolvasása ---
    int size = width * height;
    clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE, 0,
        sizeof(int) * size, hostDist.data(), 0, nullptr, nullptr);
    clEnqueueReadBuffer(clm->getQueue(), parentBuf, CL_TRUE, 0,
        sizeof(int) * size, hostParent.data(), 0, nullptr, nullptr);

    int hostNextSize = 0;
    clEnqueueReadBuffer(clm->getQueue(), nextSizeBuf, CL_TRUE, 0,
        sizeof(int), &hostNextSize, 0, nullptr, nullptr);

    // --- Ping-pong swap ---
    std::swap(frontierBuf, nextFrontierBuf);
    hostFrontierSize = hostNextSize;
    currentDist++;

    // --- Leállási feltételek ---
    if (hostDist[goalIndex] != -1)
    {
        reconstructPath(goalIndex);
        foundFlag = true;
        done = true;
        return true;
    }

    if (hostNextSize == 0)
    {
        foundFlag = false;
        done = true;
        return true;
    }

    if (currentDist > size)
    {
        foundFlag = false;
        done = true;
    }

    return done;
}

void GPUBFS::reconstructPath(int goal)
{
    hostPath.clear();
    int cur = goal;
    while (cur != -1)
    {
        hostPath.push_back(cur);
        cur = hostParent[cur];
    }
}