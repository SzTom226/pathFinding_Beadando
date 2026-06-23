#include "Maze.h"
#include <random>
#include <queue>
#include <chrono>

static unsigned makeRandomSeed()
{
    std::random_device rd;
    unsigned rdValue = rd();

    auto timeValue = static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());

    return rdValue ^ timeValue;
}

Maze::Maze(int w, int h)
    : width(w), height(h), grid(w* h, WALL)
{
}

uint8_t Maze::get(int x, int y) const
{
    return grid[index(x, y)];
}

void Maze::set(int x, int y, uint8_t value)
{
    grid[index(x, y)] = value;
}

int Maze::index(int x, int y) const
{
    return y * width + x;
}

void Maze::setWall(int x, int y) { set(x, y, WALL); }
void Maze::setEmpty(int x, int y) { set(x, y, EMPTY); }

void Maze::generateSimple()
{
    for (auto& cell : grid)
        cell = EMPTY;

    for (int x = 0; x < width; x++)
    {
        set(x, 0, WALL);
        set(x, height - 1, WALL);
    }
    for (int y = 0; y < height; y++)
    {
        set(0, y, WALL);
        set(width - 1, y, WALL);
    }

    for (int x = 2; x < width - 2; x += 2)
        for (int y = 2; y < height - 2; y++)
            if ((x + y) % 3 == 0)
                set(x, y, WALL);

    set(1, 1, START);
    set(width - 2, height - 2, GOAL);
}

void Maze::generateRandom(float wallDensity, unsigned seed)
{
    std::mt19937 rng(seed != 0 ? seed : std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const int maxAttempts = 100;
    const int sx = 1, sy = 1;
    const int gx = width - 2, gy = height - 2;

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        // alap: minden EMPTY
        for (auto& cell : grid)
            cell = EMPTY;

        // keret fal
        for (int x = 0; x < width; x++)
        {
            set(x, 0, WALL);
            set(x, height - 1, WALL);
        }
        for (int y = 0; y < height; y++)
        {
            set(0, y, WALL);
            set(width - 1, y, WALL);
        }

        // belső véletlen falak
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (dist(rng) < wallDensity)
                    set(x, y, WALL);
            }
        }

        // start + goal mindig szabad
        set(sx, sy, START);
        set(gx, gy, GOAL);

        // van-e érvényes út? ha igen, kész
        if (isReachable(sx, sy, gx, gy)) {
            return;
        }
    }

    carvePath(sx, sy, gx, gy);

    // ha sok próbálkozás után sem talált megoldhatót,
    // vágjunk ki egy garantált utat
    carvePath(sx, sy, gx, gy);
}

bool Maze::isReachable(int sx, int sy, int gx, int gy) const
{
    std::vector<bool> visited(width * height, false);
    std::queue<std::pair<int, int>> q;

    q.push({ sx, sy });
    visited[index(sx, sy)] = true;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty())
    {
        auto [cx, cy] = q.front();
        q.pop();

        if (cx == gx && cy == gy)
            return true;

        for (int dir = 0; dir < 4; dir++)
        {
            int nx = cx + dx[dir];
            int ny = cy + dy[dir];

            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            int idx = index(nx, ny);
            if (visited[idx] || get(nx, ny) == WALL)
                continue;

            visited[idx] = true;
            q.push({ nx, ny });
        }
    }

    return false;
}

void Maze::carvePath(int sx, int sy, int gx, int gy)
{
    int x = sx, y = sy;

    while (x != gx)
    {
        x += (gx > x) ? 1 : -1;
        if (get(x, y) == WALL)
            set(x, y, EMPTY);
    }

    while (y != gy)
    {
        y += (gy > y) ? 1 : -1;
        if (get(x, y) == WALL)
            set(x, y, EMPTY);
    }

    set(sx, sy, START);
    set(gx, gy, GOAL);
}