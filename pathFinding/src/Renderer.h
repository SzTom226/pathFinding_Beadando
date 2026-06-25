#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include "Maze.h"
#include "BFSBase.h"

class Renderer {
public:
    Renderer(int w, int h, int mw, int mh);
    ~Renderer();

    bool init();
    bool shouldClose() const;
    void beginFrame();
    void endFrame();

    GLFWwindow* getWindow() const { return window; }

    // Interop init – OpenCL context létrehozása UTÁN hívandó
    bool initInterop(cl_context clCtx, cl_command_queue clQueue);

    // Az OpenCL color kernel ebbe a shared image-be ír
    cl_mem getColorImage() const { return clColorImage; }

    // Egyetlen draw call: a colorTex tartalmát kirakja a képernyőre
    void drawMazeInterop();

private:
    int windowWidth, windowHeight;
    int mazeWidth, mazeHeight;
    GLFWwindow* window = nullptr;

    // fullscreen quad + texture shader
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint texShader = 0;
    GLuint colorTex = 0;   // GL_TEXTURE_2D, mazeWidth x mazeHeight, RGBA8

    // OpenCL interop
    cl_context       clCtx = nullptr;
    cl_command_queue clQueue = nullptr;
    cl_mem           clColorImage = nullptr; // clCreateFromGLTexture

    bool initTextureShader();
    bool initFullscreenQuad();
};