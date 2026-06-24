#pragma once

#include "BFSBase.h"
#include "Maze.h"
#include "OpenCLManager.h"

#include <CL/cl.h>
#include <vector>

class GPUBFS : public BFSBase
{
public:
    ~GPUBFS() override;
    void initialize(const Maze& maze) override;
    void setOpenCL(OpenCLManager& manager);
    bool step(const Maze& maze) override;
    bool finished() const override { return done; }
    bool pathFound() const override { return foundFlag; }
    const std::vector<int>& getDistances() const override { return hostDist; }
    const std::vector<int>& getPath() const override { return hostPath; }

private:
    void reconstructPath(int goal);
    void releaseClResources();

private:
    OpenCLManager* clm = nullptr;
    int width = 0;
    int height = 0;
    int currentDist = 0;
    int startIndex = -1;
    int goalIndex = -1;
    bool done = false;
    bool foundFlag = false;

    cl_program program = nullptr;
    cl_kernel kernel = nullptr;

    cl_mem mazeBuf = nullptr;
    cl_mem distBuf = nullptr;
    cl_mem parentBuf = nullptr;
    cl_mem frontierBuf = nullptr;
    cl_mem nextFrontierBuf = nullptr;
    cl_mem nextCountBuf = nullptr;

    std::vector<int> hostDist;
    std::vector<int> hostParent;
    std::vector<int> hostPath;
    std::vector<int> zeroFrontier;
};