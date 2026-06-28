#include "Maze.h"
#include <random>
#include <queue>
#include <chrono>

// Véletlenszerű seed generálása: std::random_device és a jelenlegi időbélyeg
// XOR-olásával, hogy akkor is különböző legyen, ha a hardveres entrópia korlátozott.
static unsigned makeRandomSeed()
{
    std::random_device rd;
    unsigned rdValue = rd();

    auto timeValue = static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());

    return rdValue ^ timeValue;
}

// Minden cellát WALL-ra inicializál; a tényleges tartalom generate*() híváskor töltődik fel.
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

// -----------------------------------------------------------------------------
// generateSimple
// Determinisztikus labirintus: minden belső cella EMPTY, keret WALL,
// majd (x+y) % 3 == 0 feltétellel szór el extra falakat a belső területen.
// Nem garantál egyedi megoldást, de gyors és mindig összefüggő.
// -----------------------------------------------------------------------------
void Maze::generateSimple()
{
    for (auto& cell : grid)
        cell = EMPTY;

    // Keretet fallal zárja körbe
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

    // Szabályos mintájú belső falak (minden páros x-nél, ha (x+y)%3==0)
    for (int x = 2; x < width - 2; x += 2)
        for (int y = 2; y < height - 2; y++)
            if ((x + y) % 3 == 0)
                set(x, y, WALL);

    set(1, 1, START);
    set(width - 2, height - 2, GOAL);
}

// -----------------------------------------------------------------------------
// generateRandom
// Véletlenszerű falakat szór el wallDensity valószínűséggel, majd BFS-sel
// ellenőrzi az összefüggőséget. Legfeljebb maxAttempts kísérlet után,
// ha egyik sem járható, carvePath() garantált utat vág.
// -----------------------------------------------------------------------------
void Maze::generateRandom(float wallDensity, unsigned seed)
{
    std::mt19937 rng(seed != 0 ? seed : std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const int maxAttempts = 100;
    const int sx = 1, sy = 1;
    const int gx = width - 2, gy = height - 2;

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        // Alapállapot: minden belső cella EMPTY
        for (auto& cell : grid)
            cell = EMPTY;

        // Keret falakkal körülvéve
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

        // Véletlen belső falak elhelyezése
        for (int y = 1; y < height - 1; y++)
            for (int x = 1; x < width - 1; x++)
                if (dist(rng) < wallDensity)
                    set(x, y, WALL);

        // START és GOAL pozíciója mindig szabad marad
        set(sx, sy, START);
        set(gx, gy, GOAL);

        // Ha van érvényes út, a generálás kész
        if (isReachable(sx, sy, gx, gy))
            return;
    }

    // Összes kísérlet sikertelen: garantált L-alakú út bevágása
    carvePath(sx, sy, gx, gy);
}

// -----------------------------------------------------------------------------
// isReachable
// BFS a rácskoordinátákon: WALL cellákat nem lép át.
// Visszatér true-val, ha (gx, gy) elérhető (sx, sy)-ból.
// -----------------------------------------------------------------------------
bool Maze::isReachable(int sx, int sy, int gx, int gy) const
{
    std::vector<bool> visited(width * height, false);
    std::queue<std::pair<int, int>> q;

    q.push({ sx, sy });
    visited[index(sx, sy)] = true;

    const int dx[4] = { 1, -1,  0,  0 };
    const int dy[4] = { 0,  0,  1, -1 };

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

// -----------------------------------------------------------------------------
// carvePath
// Garantált út vágása L-alakban: először vízszintesen (sx → gx),
// majd függőlegesen (sy → gy). WALL cellákat EMPTY-re állít menet közben.
// A végén visszaállítja START és GOAL értékeket, nehogy felülíródjananak.
// -----------------------------------------------------------------------------
void Maze::carvePath(int sx, int sy, int gx, int gy)
{
    int x = sx, y = sy;

    // Vízszintes szakasz
    while (x != gx)
    {
        x += (gx > x) ? 1 : -1;
        if (get(x, y) == WALL)
            set(x, y, EMPTY);
    }

    // Függőleges szakasz
    while (y != gy)
    {
        y += (gy > y) ? 1 : -1;
        if (get(x, y) == WALL)
            set(x, y, EMPTY);
    }

    set(sx, sy, START);
    set(gx, gy, GOAL);
}