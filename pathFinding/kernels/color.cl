// Bemenetek:
//   maze      – cella típusok (WALL=0, EMPTY=1, START=2, GOAL=3)
//   distance  – BFS távolságok (-1 = nem látogatott)
//   path      – útvonal celláinak indexlistája (goal->start)
//   pathLen   – hány cella van az útvonalban
//   revealCount – ebből mennyi legyen megjelenítve (animáció)
//   colorOut  – a megosztott GL textúra (image2d_t), ebbe írunk
//   width, height

__constant float4 COLOR_WALL    = (float4)(0.08f, 0.08f, 0.08f, 1.f); // sötétszürke
__constant float4 COLOR_EMPTY   = (float4)(0.95f, 0.95f, 0.95f, 1.f); // majdnem fehér
__constant float4 COLOR_START   = (float4)(0.18f, 0.80f, 0.44f, 1.f); // zöld
__constant float4 COLOR_GOAL    = (float4)(0.91f, 0.30f, 0.24f, 1.f); // piros
__constant float4 COLOR_VISITED = (float4)(0.53f, 0.81f, 0.98f, 1.f); // világoskék
__constant float4 COLOR_PATH    = (float4)(1.00f, 0.65f, 0.00f, 1.f); // narancssárga

__kernel void color_maze(
__global const uchar* maze, 
__global const int* distance, 
__global const int* path,
const int pathLen, const int revealCount, 
__write_only image2d_t colorOut, 
const int width, const int height) {
	int x = get_global_id(0);
	int y = get_global_id(1);

	if (x >= width || y >= height) return;

	int idx = y * width + x;
	uchar cell = maze[idx];

	float4 color;
	if (cell == 2) color = COLOR_START;
	else if (cell == 3) color = COLOR_GOAL;
	else if (cell == 0) color = COLOR_WALL;
	else if (distance[idx] >= 0) color = COLOR_VISITED;
	else color = COLOR_EMPTY;

	// --- Útvonal felülírja (animált reveal) ---
    // A path tömb goal->start sorrendű; start felőli animációhoz
    // hátulról olvassuk: path[pathLen-1-i] az i-edik animált cella
	for (int i = 0; i < revealCount && i < pathLen; i++) {
		if (path[pathLen - 1 - i] == idx) {
			if (cell != 2 && cell != 3)
				color = COLOR_PATH;
			break;
		}
	}

	// --- Írás a megosztott GL textúrába ---
    // Az image koordináta: (x, y), az OpenGL y-tengelye felfelé mutat,
    // de GL_NEAREST filterrel és a quad UV-vel összhangban van.
	int2 coord = (int2)(x, y);
    write_imagef(colorOut, coord, color);
}