#include "Renderer.h"
#include <iostream>

// -----------------------------------------------------------------------
// Vertex shader: fullscreen quad, tex coords (0,0)-(1,1)
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
// Fragment shader: textúra lookup → pixel szín
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
bool Renderer::init()
{
    if (!glfwInit()) { std::cout << "GLFW init failed\n"; return false; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Maze BFS – GPU Interop", nullptr, nullptr);
    if (!window) { std::cout << "Window creation failed\n"; return false; }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "GLAD init failed\n"; return false;
    }

    glViewport(0, 0, windowWidth, windowHeight);

    return initTextureShader() && initFullscreenQuad();
}

// -----------------------------------------------------------------------
// Interop init – az OpenCL context létrehozása UTÁN hívandó
// -----------------------------------------------------------------------
bool Renderer::initInterop(cl_context ctx, cl_command_queue queue)
{
    clCtx = ctx;
    clQueue = queue;

    // 1) OpenGL textúra: mazeWidth x mazeHeight, RGBA8
    //    Minden pixel = egy labirintus-cella színe
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        mazeWidth, mazeHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 2) OpenCL image2d – UGYANAZ a GPU memória mint colorTex
    cl_int err = CL_SUCCESS;
    clColorImage = clCreateFromGLTexture(
        clCtx,
        CL_MEM_WRITE_ONLY,   // az OpenCL csak ír bele
        GL_TEXTURE_2D,
        0,                   // mip level
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
bool Renderer::initTextureShader()
{
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    texShader = glCreateProgram();
    glAttachShader(texShader, vert);
    glAttachShader(texShader, frag);
    glLinkProgram(texShader);
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
bool Renderer::initFullscreenQuad()
{
    // NDC fullscreen quad, két háromszög
    // layout: x, y, u, v
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

    // aPos (location 0): x, y
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // aUV (location 1): u, v
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

// -----------------------------------------------------------------------
bool Renderer::shouldClose() const { return glfwWindowShouldClose(window); }

void Renderer::beginFrame()
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::endFrame()
{
    glfwSwapBuffers(window);
    glfwPollEvents();
}

// -----------------------------------------------------------------------
// drawMazeInterop
//   – az OpenCL color kernel már kitöltötte a colorTex-et
//   – egyetlen fullscreen quad draw call rajzolja ki
// -----------------------------------------------------------------------
void Renderer::drawMazeInterop()
{
    glUseProgram(texShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glUniform1i(glGetUniformLocation(texShader, "uTex"), 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);   // 1 draw call az egész rácshoz
    glBindVertexArray(0);
}