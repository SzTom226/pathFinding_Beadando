// ============================================================================
// color.cl – color_maze kernel
//
// A labirintus aktuális állapotát (falak, BFS-távolságok, megtalált
// útvonal) közvetlenül egy OpenGL textúrába rajzolja ki, OpenCL/OpenGL
// interop segítségével (a colorOut image2d_t ugyanaz a GPU-memória, mint
// a Renderer által megjelenített GL textúra) – így nincs szükség
// CPU-GPU adatmozgásra a vizualizációhoz.
//
// Minden work-item pontosan egy cellát (pixelt) színez ki, a 2D NDRange
// mérete megegyezik a labirintus rácsméretével (width x height).
//
// Bemenetek:
//   maze      – cella típusok (WALL=0, EMPTY=1, START=2, GOAL=3)
//   distance  – BFS távolságok (-1 = nem látogatott)
//   path      – útvonal celláinak indexlistája (goal->start)
//   pathLen   – hány cella van az útvonalban
//   revealCount – ebből mennyi legyen megjelenítve (animáció)
//   colorOut  – a megosztott GL textúra (image2d_t), ebbe írunk
//   width, height
// ============================================================================

// Színpaletta a cellatípusokhoz/állapotokhoz (RGBA, 0..1 tartományban)
__constant float4 COLOR_WALL    = (float4)(0.08f, 0.08f, 0.08f, 1.f);  // fal – sötét
__constant float4 COLOR_EMPTY   = (float4)(0.95f, 0.95f, 0.95f, 1.f);  // szabad, még nem érintett cella – világos
__constant float4 COLOR_START   = (float4)(0.18f, 0.80f, 0.44f, 1.f);  // start cella – zöld
__constant float4 COLOR_GOAL    = (float4)(0.91f, 0.30f, 0.24f, 1.f);  // cél cella – piros
__constant float4 COLOR_VISITED = (float4)(0.53f, 0.81f, 0.98f, 1.f);  // a BFS már bejárta – kék
__constant float4 COLOR_PATH    = (float4)(1.00f, 0.65f, 0.00f, 1.f);  // a végső útvonal része – narancs

__kernel void color_maze(
    __global const uchar* maze,
    __global const int*   distance,
    __global const int*   pathMask,   // pathMask[idx] = animáció-sorszám (0..pathLen-1),
                                      // vagy -1 ha nem része az útvonalnak
    const int             revealCount,
    __write_only image2d_t colorOut,
    const int             width,
    const int             height
)
{
    // A work-item 2D koordinátája megfelel egy cella (pixel) helyének
    int x = get_global_id(0);
    int y = get_global_id(1);

    // Rácson kívüli work-item-ek kihagyása (ha a globális méret kerekítve van)
    if (x >= width || y >= height) return;

    int idx = y * width + x;
    uchar cell = maze[idx];

    // Alapszín meghatározása a cella típusa/állapota alapján:
    // START/GOAL mindig kiemelt színt kap, falaknál a wall-szín,
    // a BFS által már bejárt (distance >= 0) celláknál a "visited" szín,
    // egyébként pedig az alap "empty" szín.
    float4 color;
    if      (cell == 2) color = COLOR_START;
    else if (cell == 3) color = COLOR_GOAL;
    else if (cell == 0) color = COLOR_WALL;
    else if (distance[idx] >= 0) color = COLOR_VISITED;
    else                         color = COLOR_EMPTY;

    // O(1) lookup: ha ez a cella az útvonal revealCount-on belüli részén
    // van (azaz már "fel van fedve" az animáció szerint), felülírjuk az
    // útvonal-színnel – kivéve a START/GOAL cellákat, azok megtartják
    // a saját kiemelt színüket.
    int maskVal = pathMask[idx];
    if (maskVal >= 0 && maskVal < revealCount && cell != 2 && cell != 3)
        color = COLOR_PATH;

    // Az eredmény kiírása a megosztott GL textúrába (image2d_t)
    write_imagef(colorOut, (int2)(x, y), color);
}
