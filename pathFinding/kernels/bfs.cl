// ============================================================================
// bfs.cl – bfs_step kernel
//
// Egy BFS-kör (egy "hullámfront-réteg" kiterjesztése) GPU-s, párhuzamos
// implementációja. A host (GPUBFS::step) minden lépésben egyszer indítja
// el ezt a kernelt, annyi work-itemmel, ahány cella jelenleg a frontier
// listában van.
//
// Minden work-item egy frontier-cellát dolgoz fel: megnézi mind a négy
// szomszédját, és ha egy szomszéd járható (nem fal) és még nincs
// felfedezve (distance == -1), akkor:
//   - atomikusan beállítja a távolságát (atomic_cmpxchg, hogy két
//     work-item ne írhassa be ugyanazt a cellát kétszer a next frontier-be),
//   - feljegyzi a szülőjét (parent),
//   - felveszi a kompakt "next frontier" listába egy atomikus
//     számláló (atomic_inc) által kiosztott szabad helyre.
//
// Mivel minden work-item csak a saját frontier-celláját és annak
// szomszédjait írja, és az ütközéseket atomikus műveletek oldják fel,
// nincs szükség zárolásra vagy szinkronizációra a work-item-ek között.
// ============================================================================
__kernel void bfs_step(
    __global const uchar* maze,
    __global int*         distance,
    __global int*         parent,
    __global const int*   frontierList,   // aktív cellák indexei
    const int             frontierSize,   // hány aktív cella van
    __global int*         nextFrontierList,
    __global int*         nextSize,       // atomic counter
    const int             width,
    const int             height,
    const int             currentDist
)
{
    int tid = get_global_id(0);
    // Annyi work-item indul, amennyi a globális méret (a host kerekíthet
    // felfelé) – ha ez több, mint a tényleges frontier méret, a felesleges
    // szálak nem csinálnak semmit.
    if (tid >= frontierSize) return;

    // Minden work-item egy frontier-cellát kezel
    int id = frontierList[tid];

    // 1D index -> 2D koordináta
    int x = id % width;
    int y = id / width;

    // 4-szomszédság (jobbra, balra, le, fel)
    const int dx[4] = {  1, -1,  0,  0 };
    const int dy[4] = {  0,  0,  1, -1 };

    // A szomszédok a jelenlegi rétegnél eggyel nagyobb távolságra lesznek
    const int newDist = currentDist + 1;

    for (int k = 0; k < 4; k++)
    {
        int nx = x + dx[k];
        int ny = y + dy[k];

        // Rácson kívüli szomszéd kihagyása
        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            continue;

        int ni = ny * width + nx;

        // fal = 0 (WALL) -> nem járható
        if (maze[ni] == 0)
            continue;

        // Atomic CAS: csak az első writer jut be, race condition-mentes.
        // Megnézzük, hogy distance[ni] még -1 (nem látogatott) – ha igen,
        // egyetlen atomi lépésben newDist-re állítjuk. Ha közben egy másik
        // work-item már beállította, az old != -1 lesz, és kihagyjuk.
        int old = atomic_cmpxchg(&distance[ni], -1, newDist);

        if (old == -1)
        {
            // Csak az "nyert" a cellán, aki elsőként írta be a távolságot,
            // így a parent biztonságosan, ütközés nélkül írható.
            parent[ni] = id;

            // Slot foglalás a nextFrontierList-ben: az atomic_inc minden
            // work-item-nek egyedi, ütközésmentes indexet ad.
            int slot = atomic_inc(nextSize);
            nextFrontierList[slot] = ni;
        }
    }
}
