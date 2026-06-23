#pragma once
#include <vector>
#include <cstdint>

enum Cell : uint8_t
{
	WALL = 0,
	EMPTY = 1,
	START = 2,
	GOAL = 3
};

class Maze {
public:
	Maze(int width, int height);

	void generateSimple();
	void generateRandom(float wallDensity = 0.3f, unsigned seed = 0);

	void setWall(int x, int y);
	void setEmpty(int x, int y);

	uint8_t get(int x, int y) const;
	void set(int x, int y, uint8_t value);

	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int index(int x, int y) const;

	const std::vector<uint8_t>& data() const { return grid; }
	std::vector<uint8_t>& data() { return grid; }
private:
	bool isReachable(int sx, int sy, int gx, int gy) const;
	void carvePath(int sx, int sy, int gx, int gy);

	int width;
	int height;
	std::vector<uint8_t> grid;
};