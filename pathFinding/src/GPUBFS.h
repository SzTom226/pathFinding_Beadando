#pragma once

#include "BFSBase.h"
#include "Maze.h"
#include "OpenCLManager.h"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <vector>
#include <string>

class GPUBFS : public BFSBase
{
public:
    ~GPUBFS() override;

    void setOpenCL(OpenCLManager& manager);

    void initialize(const Maze& maze) override;
    bool step(const Maze& maze) override;

    bool finished()   const override { return done; }
    bool pathFound()  const override { return foundFlag; }
    const std::vector<int>& getDistances() const override { return hostDist; }
    const std::vector<int>& getPath()      const override { return hostPath; }

    // Interop: a color kernel futtatása a megosztott GL textúrára
    // colorImage = Renderer::getColorImage()
    void updateColorTexture(cl_mem colorImage, int revealCount);

private:
    void reconstructPath(int goal);
    void releaseClResources();
    bool buildProgram(const std::string& src, const std::string& label,
        cl_program& outProg, cl_kernel& outKernel,
        const char* kernelName);

private:
    OpenCLManager* clm = nullptr;
    int width = 0, height = 0;
    int currentDist = 0;
    int startIndex = -1;
    int goalIndex = -1;
    bool done = false;
    bool foundFlag = false;

    // BFS kernel
    cl_program bfsProgram = nullptr;
    cl_kernel  bfsKernel = nullptr;

    // Color kernel
    cl_program colorProgram = nullptr;
    cl_kernel  colorKernel = nullptr;

    // BFS bufferek
    cl_mem mazeBuf = nullptr;
    cl_mem distBuf = nullptr;
    cl_mem parentBuf = nullptr;
    cl_mem frontierBuf = nullptr;
    cl_mem frontierSizeBuf = nullptr;
    cl_mem nextFrontierBuf = nullptr;
    cl_mem nextSizeBuf = nullptr;

    // pathMask: pathMask[idx] = animáció-sorszám ha az idx rajta van az útvonalon,
    // különben -1. A color kernel O(1)-ben nézi fel, nincs loop.
    cl_mem pathMaskBuf = nullptr;
    int    hostPathLen = 0;

    std::vector<int> hostDist;
    std::vector<int> hostParent;
    std::vector<int> hostPath;

    int hostFrontierSize = 0;
};