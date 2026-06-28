#include "BFS.h"
#include <iostream>

// -----------------------------------------------------------------------
// initialize
//   Végigpásztázza a labirintust, megkeresi a START és GOAL cellák
//   indexét, majd nullázza/feltölti a BFS belső állapotát: minden cella
//   távolsága -1 (nem látogatott), kivéve a startot (0), a frontier-be
//   pedig csak a start cella kerül bele.
// -----------------------------------------------------------------------
void BFS::initialize(const Maze& maze)
{
    width = maze.getWidth();
    height = maze.getHeight();

    int size = width * height;

    // Minden cella "nem látogatott" állapotba kerül
    dist.assign(size, -1);
    parent.assign(size, -1);
    path.clear();

    // Egy korábbi futásból megmaradt elemek törlése a frontier-ből
    while (!frontier.empty()) frontier.pop();

    finishedFlag = false;
    foundFlag = false;

    startIndex = -1;
    goalIndex = -1;

    // START/GOAL cellák megkeresése a rácsban
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int idx = y * width + x;

            if (maze.get(x, y) == START)
                startIndex = idx;

            if (maze.get(x, y) == GOAL)
                goalIndex = idx;
        }

    // A start cella távolsága 0, és ez az egyetlen kezdő frontier-elem
    dist[startIndex] = 0;
    frontier.push(startIndex);
}

// -----------------------------------------------------------------------
// step
//   Egyetlen BFS-lépés: kivesszük a frontier elejéről a soron következő
//   cellát, és ha ez a GOAL, kész az út. Egyébként megnézzük mind a négy
//   szomszédját (jobb/bal/le/fel), és a még nem látogatottakat
//   (dist == -1) felvesszük a frontier végére, beállítva a távolságukat
//   és a szülőjüket (az útvonal-rekonstrukcióhoz).
//
//   Visszatérési érték: true, ha az algoritmus ezzel a lépéssel lezárult
//   (megtalálta a célt VAGY kiürült a frontier, tehát a cél elérhetetlen).
// -----------------------------------------------------------------------
bool BFS::step(const Maze& maze)
{
    if (finishedFlag) return true;

    // Ha üres a frontier, nincs több felfedezhető cella -> nincs út a célhoz
    if (frontier.empty()) return finishedFlag = true;

    int cur = frontier.front();
    frontier.pop();


    // Célba értünk: rekonstruáljuk az utat és jelezzük a sikert
    if (cur == goalIndex)
    {
        reconstructPath(cur);
        foundFlag = true;
        finishedFlag = true;
        return true;
    }

    // 1D index -> 2D koordináta (x, y)
    int cx = cur % width;
    int cy = cur / width;

    // 4-szomszédság (jobbra, balra, le, fel)
    const int dx[4] = { 1,-1,0,0 };
    const int dy[4] = { 0,0,1,-1 };

    for (int i = 0; i < 4; i++)
    {
        int nx = cx + dx[i];
        int ny = cy + dy[i];

        // Rácson kívüli szomszéd kihagyása
        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            continue;

        // Fal -> nem járható cella
        if (maze.get(nx, ny) == WALL)
            continue;

        int ni = ny * width + nx;

        // Csak akkor vesszük fel a frontier-be, ha még nem jártunk ott
        if (dist[ni] == -1)
        {
            dist[ni] = dist[cur] + 1;
            parent[ni] = cur;
            frontier.push(ni);
        }
    }

    return false;
}

// -----------------------------------------------------------------------
// reconstructPath
//   A parent-tömb mentén visszalépkedünk a goal cellától a start celláig
//   (amelynek parent értéke -1), és ezzel építjük fel a path tömböt
//   GOAL -> START sorrendben.
// -----------------------------------------------------------------------
void BFS::reconstructPath(int goal)
{
    path.clear();
    int cur = goal;

    while (cur != -1)
    {
        path.push_back(cur);
        cur = parent[cur];
    }
}