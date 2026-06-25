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
    bool finished() const override { return done; }
    bool pathFound() const override { return foundFlag; }
    const std::vector<int>& getDistances() const override { return hostDist; }
    const std::vector<int>& getPath() const override { return hostPath; }
    void updateColorTexture(cl_mem colorImage, int revealCount);

private:
    void reconstructPath(int goal);
    void releaseClResources();
    bool buildProgram(const std::string& src, const std::string& label, cl_program& outProg, cl_kernel& outKernel, const char* kernelName);

private:
    OpenCLManager* clm = nullptr;
    int width = 0;
    int height = 0;
    int currentDist = 0;
    int startIndex = -1;
    int goalIndex = -1;
    bool done = false;
    bool foundFlag = false;

    // BFS kernel
    cl_program bfsProgram = nullptr;
    cl_kernel bfsKernel = nullptr;

    // Color kernel
    cl_program colorProgram = nullptr;
    cl_kernel colorKernel = nullptr;

    // BFS bufferek
    cl_mem mazeBuf = nullptr;
    cl_mem distBuf = nullptr;
    cl_mem parentBuf = nullptr;
    // compact frontier: indexlista + méret
    cl_mem frontierBuf = nullptr;   // int[] – aktív cellák indexei
    cl_mem frontierSizeBuf = nullptr;   // int  – hány aktív cella van
    cl_mem nextFrontierBuf = nullptr;   // int[] – következő szint indexei
    cl_mem nextSizeBuf = nullptr;   // int  – atomic counter a következő szinthez

    // Color bufferek
    cl_mem pathBuf = nullptr;
    int hostPathLen = 0;

    std::vector<int> hostDist;
    std::vector<int> hostParent;
    std::vector<int> hostPath;

    int hostFrontierSize = 0;
};