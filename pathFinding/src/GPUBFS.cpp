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
    if (kernel) { clReleaseKernel(kernel); kernel = nullptr; }
    if (program) { clReleaseProgram(program); program = nullptr; }
    if (mazeBuf) { clReleaseMemObject(mazeBuf); mazeBuf = nullptr; }
    if (distBuf) { clReleaseMemObject(distBuf); distBuf = nullptr; }
    if (parentBuf) { clReleaseMemObject(parentBuf); parentBuf = nullptr; }
    if (frontierBuf) { clReleaseMemObject(frontierBuf); frontierBuf = nullptr; }
    if (nextFrontierBuf) { clReleaseMemObject(nextFrontierBuf); nextFrontierBuf = nullptr; }
    if (nextCountBuf) { clReleaseMemObject(nextCountBuf); nextCountBuf = nullptr; }
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

    zeroFrontier.assign(size, 0);

    std::vector<int> frontier(size, 0);

    startIndex = -1;
    goalIndex = -1;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = y * width + x;

            if (maze.get(x, y) == START)
                startIndex = idx;

            if (maze.get(x, y) == GOAL)
                goalIndex = idx;
        }
    }

    frontier[startIndex] = 1;
    hostDist[startIndex] = 0;

    currentDist = 0;
    done = false;
    foundFlag = false;

    mazeBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        size * sizeof(uint8_t), (void*)maze.data().data(), nullptr);

    distBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostDist.data(), nullptr);

    parentBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), hostParent.data(), nullptr);

    frontierBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), frontier.data(), nullptr);

    nextFrontierBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        size * sizeof(int), zeroFrontier.data(), nullptr);

    nextCountBuf = clCreateBuffer(clm->getContext(), CL_MEM_READ_WRITE,
        sizeof(int), nullptr, nullptr);

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
        clGetProgramBuildInfo(program, clm->getDevice(), CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
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
    if (done)
        return true;

    if (!kernel)
    {
        std::cout << "GPUBFS::step() - nincs ervenyes kernel (lasd az initialize() "
            "hibait fentebb), a BFS nem tud futni.\n";
        done = true;
        foundFlag = false;
        return true;
    }

    const size_t global = static_cast<size_t>(width) * static_cast<size_t>(height);

    clEnqueueWriteBuffer(clm->getQueue(), nextFrontierBuf, CL_TRUE, 0,
        sizeof(int) * global, zeroFrontier.data(), 0, nullptr, nullptr);

    int zero = 0;
    clEnqueueWriteBuffer(clm->getQueue(), nextCountBuf, CL_TRUE, 0,
        sizeof(int), &zero, 0, nullptr, nullptr);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &mazeBuf);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &distBuf);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &parentBuf);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &frontierBuf);
    clSetKernelArg(kernel, 4, sizeof(cl_mem), &nextFrontierBuf);
    clSetKernelArg(kernel, 5, sizeof(cl_mem), &nextCountBuf);
    clSetKernelArg(kernel, 6, sizeof(int), &width);
    clSetKernelArg(kernel, 7, sizeof(int), &height);
    clSetKernelArg(kernel, 8, sizeof(int), &currentDist);

    clEnqueueNDRangeKernel(clm->getQueue(), kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
    clFinish(clm->getQueue());

    clEnqueueReadBuffer(clm->getQueue(), distBuf, CL_TRUE, 0, sizeof(int) * global, hostDist.data(), 0, nullptr, nullptr);
    clEnqueueReadBuffer(clm->getQueue(), parentBuf, CL_TRUE, 0, sizeof(int) * global, hostParent.data(), 0, nullptr, nullptr);

    int hostNextCount = 0;
    clEnqueueReadBuffer(clm->getQueue(), nextCountBuf, CL_TRUE, 0, sizeof(int), &hostNextCount, 0, nullptr, nullptr);

    std::swap(frontierBuf, nextFrontierBuf);
    currentDist++;

    if (hostDist[goalIndex] != -1)
    {
        reconstructPath(goalIndex);
        foundFlag = true;
        done = true;
        return true;
    }

    if (hostNextCount == 0)
    {
        foundFlag = false;
        done = true;
        return true;
    }

    if (currentDist > width * height)
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