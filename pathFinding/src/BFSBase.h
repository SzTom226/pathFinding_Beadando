#pragma once

#include <vector>

class Maze;

class BFSBase
{
public:
    virtual ~BFSBase() = default;

    virtual void initialize(const Maze& maze) = 0;
    virtual bool step(const Maze& maze) = 0;

    virtual bool finished() const = 0;
    virtual bool pathFound() const = 0;

    virtual const std::vector<int>& getDistances() const = 0;
    virtual const std::vector<int>& getPath() const = 0;
};