#include "Renderer.h"
#include <iostream>

// -----------------------------------------------------------------------
// Vertex shader: NDC koordinátákból fullscreen quadot rajzol.
// aPos: [-1,1] tartományú pozíció, aUV: [0,1] textúra koordináta.
// -----------------------------------------------------------------------
static const char* vertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// -----------------------------------------------------------------------
// Fragment shader: egyszerű textúra lookup – a colorTex pixel színét adja vissza.
// Az összes labirintus-logika (fal/üres/útvonal szín) az OpenCL color kernelben van,
// a fragment shader csak továbbítja az eredményt.
// -----------------------------------------------------------------------
static const char* fragSrc = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    FragColor = texture(uTex, vUV);
}
)";

// Lefordít egy GL shadert és hibaüzenetet nyomtat, ha a fordítás sikertelen.
static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cout << "Shader error: " << log << "\n";
    }
    return s;
}

// -----------------------------------------------------------------------
Renderer::Renderer(int w, int h, int mw, int mh)
    : windowWidth(w), windowHeight(h), mazeWidth(mw), mazeHeight(mh) {
}

// Fordított sorrendben szabadítja fel az erőforrásokat:
// CL image → GL textúra → quad geometria → shader → ablak → GLFW.
Renderer::~Renderer()
{
    if (clColorImage) clReleaseMemObject(clColorImage);
    if (colorTex)     glDeleteTextures(1, &colorTex);
    if (quadVAO)      glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO)      glDeleteBuffers(1, &quadVBO);
    if (texShader)    glDeleteProgram(texShader);
    if (window)       glfwDestroyWindow(window);
    glfwTerminate();
}

// -----------------------------------------------------------------------
// init
// OpenGL 3.3 core context létrehozása GLFW-vel, GLAD betöltése,
// majd a shader és a quad geometria inicializálása.
// -----------------------------------------------------------------------
bool Renderer::init()
{
    if (!glfwInit()) { std::cout << "GLFW init failed\n"; return false; }

    // OpenGL 3.3 core profile – kompatibilis a legtöbb modern GPU-val
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Maze BFS – GPU Interop", nullptr, nullptr);
    if (!window) { std::cout << "Window creation failed\n"; return false; }

    glfwMakeContextCurrent(window);

    // GLAD tölti be a GL függvény pointereket a driver-ből
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "GLAD init failed\n"; return false;
    }

    glViewport(0, 0, windowWidth, windowHeight);

    return initTextureShader() && initFullscreenQuad();
}

// -----------------------------------------------------------------------
// initInterop
// Létrehoz egy mazeWidth x mazeHeight méretű RGBA8 GL textúrát (colorTex),
// majd ugyanezt a GPU memóriát OpenCL image2d-ként is elérhetővé teszi
// (clColorImage). Az OpenCL color kernel ebbe ír, az OpenGL ebből olvas –
// CPU roundtrip nélkül.
// FONTOS: az OpenCL context létrehozása UTÁN hívandó.
// -----------------------------------------------------------------------
bool Renderer::initInterop(cl_context ctx, cl_command_queue queue)
{
    clCtx = ctx;
    clQueue = queue;

    // GL textúra létrehozása: minden pixel egy labirintus-cella megjelenített színe.
    // GL_NEAREST szűrés: pixel-pontos megjelenítés, nincs interpoláció.
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        mazeWidth, mazeHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Az OpenCL image2d ugyanarra a GPU memóriára mutat mint a colorTex.
    // CL_MEM_WRITE_ONLY: a color kernel csak ír bele, sosem olvas.
    cl_int err = CL_SUCCESS;
    clColorImage = clCreateFromGLTexture(
        clCtx,
        CL_MEM_WRITE_ONLY,
        GL_TEXTURE_2D,
        0,          // mip level 0
        colorTex,
        &err);

    if (err != CL_SUCCESS || !clColorImage) {
        std::cout << "clCreateFromGLTexture failed, error: " << err << "\n";
        return false;
    }

    std::cout << "OpenCL - OpenGL interop texture created ("
        << mazeWidth << "x" << mazeHeight << ")\n";
    return true;
}

// -----------------------------------------------------------------------
// initTextureShader
// Lefordítja a vertex és fragment shadert, összelinkeli a shader programot.
// Az ideiglenes shader objektumokat linkelés után azonnal törli.
// -----------------------------------------------------------------------
bool Renderer::initTextureShader()
{
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    texShader = glCreateProgram();
    glAttachShader(texShader, vert);
    glAttachShader(texShader, frag);
    glLinkProgram(texShader);

    // A shader objektumok a program-hoz csatolva tovább élnek linkelés után,
    // de külön handle-ként már nincs rájuk szükség
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok;
    glGetProgramiv(texShader, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(texShader, 512, nullptr, log);
        std::cout << "Texture shader link error: " << log << "\n";
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------
// initFullscreenQuad
// Feltölti a GPU-ra a 6 vertex adatát (2 háromszög = 1 quad).
// Layout: vec2 pos (NDC), vec2 uv – stride: 4 float.
// Az UV koordináták felülről lefele haladnak (0,0 = bal-felső).
// -----------------------------------------------------------------------
bool Renderer::initFullscreenQuad()
{
    // 6 vertex: 2 háromszög alkotja a teljes [-1,1] NDC teret
    // Oszlopok: x, y (NDC pozíció), u, v (textúra koordináta)
    float verts[] = {
        -1.f,  1.f,  0.f, 0.f,   // bal-felső
         1.f,  1.f,  1.f, 0.f,   // jobb-felső
         1.f, -1.f,  1.f, 1.f,   // jobb-alsó

        -1.f,  1.f,  0.f, 0.f,   // bal-felső
         1.f, -1.f,  1.f, 1.f,   // jobb-alsó
        -1.f, -1.f,  0.f, 1.f,   // bal-alsó
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    // location 0: vec2 aPos – NDC pozíció (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // location 1: vec2 aUV – textúra koordináta (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

// -----------------------------------------------------------------------
bool Renderer::shouldClose() const { return glfwWindowShouldClose(window); }

// Fekete háttérrel törli a framebuffert – minden frame elején hívandó.
void Renderer::beginFrame()
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// Double buffer csere és GLFW eseménykezelés – minden frame végén hívandó.
void Renderer::endFrame()
{
    glfwSwapBuffers(window);
    glfwPollEvents();
}

// -----------------------------------------------------------------------
// drawMazeInterop
// Az OpenCL color kernel már lefutott és a colorTex tartalmát feltöltötte.
// Ez a függvény egyetlen draw call-lal (6 vertex, 2 háromszög) kirajzolja
// a teljes labirintust a képernyőre – nincs per-cella CPU munka.
// -----------------------------------------------------------------------
void Renderer::drawMazeInterop()
{
    glUseProgram(texShader);

    // colorTex bekötése a shader uTex uniform-jához (texture unit 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glUniform1i(glGetUniformLocation(texShader, "uTex"), 0);

    // Fullscreen quad kirajzolása – 6 vertex = 2 háromszög = teljes képernyő
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}