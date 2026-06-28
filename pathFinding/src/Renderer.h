#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include "Maze.h"
#include "BFSBase.h"

// Kezeli az OpenGL ablakot, a fullscreen quad renderelést és az OpenCL–GL
// interop textúrát. A labirintus megjelenítése egyetlen draw call-lal történik:
// az OpenCL color kernel közvetlenül a megosztott GPU textúrába ír, amelyet
// a Renderer egy fullscreen quadon jelenít meg – CPU roundtrip nélkül.
class Renderer {
public:
    // w, h   – ablak mérete pixelben
    // mw, mh – labirintus rács mérete (textúra felbontása)
    Renderer(int w, int h, int mw, int mh);

    // Felszabadítja az összes GL és CL erőforrást, bezárja az ablakot.
    ~Renderer();

    // Inicializálja a GLFW ablakot, az OpenGL 3.3 core contextet, a GLAD-ot,
    // a textúra shadert és a fullscreen quad geometriát.
    // Visszatér false-szal, ha bármelyik lépés sikertelen.
    bool init();

    // Visszatér true-val, ha a felhasználó bezárta az ablakot.
    bool shouldClose() const;

    // Törli a framebuffert (fekete háttér) – frame elején hívandó.
    void beginFrame();

    // Megjeleníti a rendert (swap buffers) és feldolgozza az ablakesemányeket.
    void endFrame();

    // Az aktuális GLFW ablak handle-je (pl. input kezeléshez).
    GLFWwindow* getWindow() const { return window; }

    // Létrehozza a megosztott GL textúrát és a hozzá tartozó OpenCL image2d-t.
    // FONTOS: az OpenCL context létrehozása UTÁN hívandó (initInterop sorrendje:
    // init() → OpenCLManager::initialize() → initInterop()).
    bool initInterop(cl_context clCtx, cl_command_queue clQueue);

    // Az OpenCL color kernel ebbe a cl_mem image2d-be ír (ugyanaz a GPU memória
    // mint a colorTex GL textúra – clCreateFromGLTexture hozta létre).
    cl_mem getColorImage() const { return clColorImage; }

    // Kirajzolja a colorTex tartalmát egy fullscreen quadon (1 draw call).
    // Feltételezi, hogy az OpenCL color kernel már lefutott és felszabadította
    // a textúra ownership-jét (clEnqueueReleaseGLObjects).
    void drawMazeInterop();

private:
    int windowWidth, windowHeight;  // ablak mérete pixelben
    int mazeWidth, mazeHeight;    // labirintus rács mérete = textúra mérete

    GLFWwindow* window = nullptr;

    // Fullscreen quad rendereléshez szükséges GL objektumok
    GLuint quadVAO = 0;  // vertex array object
    GLuint quadVBO = 0;  // vertex buffer (6 vertex, NDC koordinátákkal)
    GLuint texShader = 0; // shader program (vertex + fragment)
    GLuint colorTex = 0; // GL_TEXTURE_2D, RGBA8, mazeWidth x mazeHeight

    // OpenCL interop handle-ek
    cl_context       clCtx = nullptr;
    cl_command_queue clQueue = nullptr;
    cl_mem           clColorImage = nullptr; // clCreateFromGLTexture eredménye

    // Lefordítja és linkeli a textúra megjelenítő shader programot.
    bool initTextureShader();

    // Feltölti a GPU-ra a fullscreen quad vertex adatait (NDC + UV koordináták).
    bool initFullscreenQuad();
};