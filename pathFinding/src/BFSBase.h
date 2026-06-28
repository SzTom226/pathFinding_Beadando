#pragma once

#include <vector>

class Maze;

// ============================================================================
// BFSBase
//
// Közös absztrakt interfész a labirintus-bejárási (legrövidebb utat kereső)
// algoritmusok számára. Ez teszi lehetővé, hogy a CPU-s (BFS) és a GPU-s
// (GPUBFS) implementáció ugyanazon a felületen keresztül legyen használható
// (pl. main.cpp-ben, illetve teszteléshez/összehasonlításhoz).
//
// Az algoritmus "lépésenkénti" (step-based) futtatásra van kialakítva:
// egy initialize() hívás után a step() metódust ismételten meghívva
// haladunk előre az algoritmusban – ez teszi lehetővé a valós idejű
// vizualizációt/animációt (minden frame-ben csak egy lépést haladunk).
// ============================================================================
class BFSBase
{
public:
    virtual ~BFSBase() = default;

    // Inicializálja/újraindítja a keresést a megadott labirintuson:
    // megkeresi a START és GOAL cellákat, feltölti a kezdő frontier-t,
    // és nullázza a belső állapotot (távolságok, szülő-tömb, stb.).
    virtual void initialize(const Maze& maze) = 0;

    // Egyetlen lépést hajt végre az algoritmusból (pl. egy BFS-hullámfront
    // kiterjesztése). Visszatérési érték: true, ha az algoritmus végzett
    // (akár mert megtalálta a célt, akár mert kifogyott a feldolgozható
    // cellákból), false, ha még van hátralévő munka.
    virtual bool step(const Maze& maze) = 0;

    // true, ha az algoritmus lefutott (a step() többé nem csinál semmit).
    virtual bool finished() const = 0;

    // true, ha a keresés során sikerült elérni a GOAL cellát.
    virtual bool pathFound() const = 0;

    // A start cellától mért BFS-távolság minden (bejárt) cellára;
    // -1, ha a cella még nincs felfedezve.
    virtual const std::vector<int>& getDistances() const = 0;

    // A megtalált legrövidebb út cellaindexei GOAL-tól START-ig
    // (path[0] = goal, path[last] = start). Csak akkor érvényes,
    // ha pathFound() == true.
    virtual const std::vector<int>& getPath() const = 0;
};