// Bemenetek:
//   maze      – cella típusok (WALL=0, EMPTY=1, START=2, GOAL=3)
//   distance  – BFS távolságok (-1 = nem látogatott)
//   path      – útvonal celláinak indexlistája (goal->start)
//   pathLen   – hány cella van az útvonalban
//   revealCount – ebből mennyi legyen megjelenítve (animáció)
//   colorOut  – a megosztott GL textúra (image2d_t), ebbe írunk
//   width, height

__constant float4 COLOR_WALL    = (float4)(0.08f, 0.08f, 0.08f, 1.f);
__constant float4 COLOR_EMPTY   = (float4)(0.95f, 0.95f, 0.95f, 1.f);
__constant float4 COLOR_START   = (float4)(0.18f, 0.80f, 0.44f, 1.f);
__constant float4 COLOR_GOAL    = (float4)(0.91f, 0.30f, 0.24f, 1.f);
__constant float4 COLOR_VISITED = (float4)(0.53f, 0.81f, 0.98f, 1.f);
__constant float4 COLOR_PATH    = (float4)(1.00f, 0.65f, 0.00f, 1.f);

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
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x >= width || y >= height) return;

    int idx = y * width + x;
    uchar cell = maze[idx];

    float4 color;
    if      (cell == 2) color = COLOR_START;
    else if (cell == 3) color = COLOR_GOAL;
    else if (cell == 0) color = COLOR_WALL;
    else if (distance[idx] >= 0) color = COLOR_VISITED;
    else                         color = COLOR_EMPTY;

    // O(1) lookup: ha ez a cella az útvonal revealCount-on belüli részén van
    int maskVal = pathMask[idx];
    if (maskVal >= 0 && maskVal < revealCount && cell != 2 && cell != 3)
        color = COLOR_PATH;

    write_imagef(colorOut, (int2)(x, y), color);
}