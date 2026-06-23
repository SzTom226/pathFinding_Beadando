#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Maze.h"

class Renderer {
public:
    Renderer(int w, int h, int mw, int mh);
    ~Renderer();

    bool init();
    bool shouldClose() const;
    void beginFrame();
    void endFrame();
    void drawCell(int x, int y, float r, float g, float b);
    void drawMaze(const Maze& maze);

private:
    int windowWidth, windowHeight;
    int mazeWidth, mazeHeight;
    GLFWwindow* window;

    GLuint shaderProgram;
    GLuint VAO, VBO;

    bool initShaders();
    bool initQuadBuffers();
};