__kernel void bfs_step(
__global const uchar* maze,
__global int* distance,
__global int* frontier,
__global int* nextFrontier,
__global int* nextCount,
const int width,
const int height,
const int currentDistance
) {
	int id = get_global_id(0);
	if (frontier[id] == 0)
		return;

	int x = id % width;
	int y = id / width;

	const int dx[4] = { 1, -1, 0, 0 };
	const int dy[4] = { 0, 0, 1, -1 };

	for (int k = 0; k < 4; k++) {
		int nx = x + dx[k];
		int ny = y + dy[k];

		if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
			continue;
		}

		int ni = ny * width + nx;

		if (maze[ni] == 0) {
			continue;
		}

		if (distance[ni] != -1) {
			continue;
		}

		distance[ni] = currentDistance + 1;
		nextFrontier[ni] = 1;
		atomic_inc(nextCount);
	}
}