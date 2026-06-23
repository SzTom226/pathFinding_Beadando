#include "Renderer.h"
#include <iostream>

static const char* vertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* fragSrc = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
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

Renderer::Renderer(int w, int h, int mw, int mh)
    : windowWidth(w), windowHeight(h),
    mazeWidth(mw), mazeHeight(mh),
    window(nullptr),
    shaderProgram(0), VAO(0), VBO(0) {
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Renderer::init() {
    if (!glfwInit()) {
        std::cout << "GLFW init failed\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Maze BFS", nullptr, nullptr);
    if (!window) {
        std::cout << "Window creation failed\n";
        return false;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "GLAD init failed\n";
        return false;
    }

    glViewport(0, 0, windowWidth, windowHeight);

    return initShaders() && initQuadBuffers();
}

bool Renderer::initShaders() {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vert);
    glAttachShader(shaderProgram, frag);
    glLinkProgram(shaderProgram);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
        std::cout << "Program link error: " << log << "\n";
        return false;
    }
    return true;
}

bool Renderer::initQuadBuffers() {
    // Egységnégyzet (két háromszög), NDC-ben [-1,1] x [-1,1]
    // A tényleges pozíciót drawCell() tölti fel minden hívásnál
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // 6 csúcs (2 háromszög), 2 float/csúcs — helyet foglalunk, adatot majd drawCell tölt
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return true;
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Renderer::beginFrame() {
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void Renderer::drawCell(int x, int y, float r, float g, float b) {
    float cellW = 2.0f / mazeWidth;
    float cellH = 2.0f / mazeHeight;

    float left = -1.0f + x * cellW;
    float right = left + cellW;
    float top = 1.0f - y * cellH;
    float bottom = top - cellH;

    // 2 háromszög = 6 csúcs
    float verts[12] = {
        left,  top,
        right, top,
        right, bottom,
        left,  top,
        right, bottom,
        left,  bottom,
    };

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glUseProgram(shaderProgram);
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform3f(colorLoc, r, g, b);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::drawMaze(const Maze& maze) {
    for (int y = 0; y < mazeHeight; y++) {
        for (int x = 0; x < mazeWidth; x++) {
            uint8_t c = maze.get(x, y);
            switch (c) {
            case WALL:  drawCell(x, y, 0.f, 0.f, 0.f); break;
            case EMPTY: drawCell(x, y, 1.f, 1.f, 1.f); break;
            case START: drawCell(x, y, 0.f, 1.f, 0.f); break;
            case GOAL:  drawCell(x, y, 1.f, 0.f, 0.f); break;
            }
        }
    }
}