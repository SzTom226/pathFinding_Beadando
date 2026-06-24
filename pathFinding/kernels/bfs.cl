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
    if (tid >= frontierSize) return;

    // Minden work-item egy frontier-cellát kezel
    int id = frontierList[tid];

    int x = id % width;
    int y = id / width;

    const int dx[4] = {  1, -1,  0,  0 };
    const int dy[4] = {  0,  0,  1, -1 };

    const int newDist = currentDist + 1;

    for (int k = 0; k < 4; k++)
    {
        int nx = x + dx[k];
        int ny = y + dy[k];

        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            continue;

        int ni = ny * width + nx;

        // fal = 0 (WALL)
        if (maze[ni] == 0)
            continue;

        // Atomic CAS: csak az első writer jut be, race condition-mentes
        int old = atomic_cmpxchg(&distance[ni], -1, newDist);

        if (old == -1)
        {
            parent[ni] = id;

            // Slot foglalás a nextFrontierList-ben
            int slot = atomic_inc(nextSize);
            nextFrontierList[slot] = ni;
        }
    }
}
