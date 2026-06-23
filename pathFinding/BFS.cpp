#include "BFS.h"
#include <queue>
#include <algorithm>

bool BFS::findPath(const Maze& maze) {
	int w = maze.getWidth();
	int h = maze.getHeight();

	dist.assign(w * h, -1);
	parent.assign(w * h, -1);
	path.clear();

	auto idx = [w](int x, int y) {
		return y * w + x;
		};
	int start = -1;
	int goal = -1;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			uint8_t cell = maze.get(x, y);
			
			if (cell == START)
			{
				start = idx(x, y);
			}

			if (cell == GOAL)
			{
				goal = idx(x, y);
			}
		}
	}

	if (start == -1 || goal == -1)
	{
		return false;
	}

	std::queue<int> q;
	dist[start] = 0;
	q.push(start);

	int dirs[4][2] =
	{
		{ 1, 0},
		{-1, 0},
		{ 0, 1},
		{ 0,-1}
	};

	while (!q.empty())
	{
		int cur = q.front();
		q.pop();

		if (cur == goal)
		{
			break;
		}

		int cx = cur % w;
		int cy = cur / w;

		for (auto& d : dirs) {
			int nx = cx + d[0];
			int ny = cy + d[1];

			if (nx < 0 || ny < 0 || nx >= w || ny >= h)
			{
				continue;
			}

			if (maze.get(nx, ny) == WALL)
				continue;

			int ni = idx(nx, ny);

			if (dist[ni] == -1)
			{
				dist[ni] = dist[cur] + 1;
				parent[ni] = cur;
				q.push(ni);
			}
		}
	}

	if (dist[goal] == -1)
		return false;

	int current = goal;

	while (current != -1)
	{
		path.push_back(current);
		current = parent[current];
	}

	std::reverse(path.begin(), path.end());

	return true;
}