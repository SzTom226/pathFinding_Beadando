#pragma once
#include <vector>
#include <cstdint>

enum Cell : uint8_t
{
	WALL = 0,  // fal – áthatolhatatlan cella
	EMPTY = 1,  // üres járható cella
	START = 2,  // kezdőpont
	GOAL = 3   // célpont
};

class Maze {
public:
	// Létrehoz egy width x height méretű labirintust, minden cellát WALL-ra inicializál.
	Maze(int width, int height);

	// Egyszerű, determinisztikus labirintust generál:
	// keretet fallal veszi körbe, belül szabályos mintázatú falakat helyez el,
	// majd a bal felső sarokba START-ot, a jobb alsóba GOAL-t tesz.
	void generateSimple();

	// Véletlenszerű labirintust generál adott falűrűséggel (0.0–1.0).
	// Legfeljebb 100-szor próbálkozik érvényes (összefüggő) labirintussal;
	// ha egyik sem sikerül, garantált L-alakú utat vág START–GOAL között.
	// seed = 0 esetén véletlenszerű seed-et használ.
	void generateRandom(float wallDensity = 0.3f, unsigned seed = 0);

	// Az (x, y) cellát WALL-ra állítja.
	void setWall(int x, int y);

	// Az (x, y) cellát EMPTY-re állítja.
	void setEmpty(int x, int y);

	// Visszaadja az (x, y) cella típusát (WALL / EMPTY / START / GOAL).
	uint8_t get(int x, int y) const;

	// Beállítja az (x, y) cella értékét.
	void set(int x, int y, uint8_t value);

	// A rács szélességét adja vissza (oszlopok száma).
	int getWidth()  const { return width; }

	// A rács magasságát adja vissza (sorok száma).
	int getHeight() const { return height; }

	// Lineáris index számítása: (x, y) → y * width + x.
	int index(int x, int y) const;

	// Közvetlen olvasási hozzáférés a belső byte-tömbhöz (GPU feltöltéshez).
	const std::vector<uint8_t>& data() const { return grid; }

	// Közvetlen írási hozzáférés a belső byte-tömbhöz.
	std::vector<uint8_t>& data() { return grid; }

private:
	// BFS-sel ellenőrzi, hogy (sx, sy)-ból elérhető-e (gx, gy)
	// a jelenlegi rácson (WALL cellákat nem lépi át).
	bool isReachable(int sx, int sy, int gx, int gy) const;

	// L-alakú garantált utat vág (sx, sy)-tól (gx, gy)-ig:
	// először vízszintesen halad gx oszlopig, majd függőlegesen gy sorig,
	// minden WALL cellát EMPTY-re állítva. Végül visszaállítja START és GOAL értékeket.
	void carvePath(int sx, int sy, int gx, int gy);

	int width;
	int height;
	std::vector<uint8_t> grid;
};