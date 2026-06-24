#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Maze.h"
#include "BFS.h"

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

    // pathRevealCount: -1 = teljes út azonnal, 0 = még semmi, N = az út első N cellája start felől
    void drawMaze(const Maze& maze, const BFSBase& bfs, int pathRevealCount = -1);

private:
    int windowWidth, windowHeight;
    int mazeWidth, mazeHeight;
    GLFWwindow* window;

    GLuint shaderProgram;
    GLuint VAO, VBO;

    bool initShaders();
    bool initQuadBuffers();
};