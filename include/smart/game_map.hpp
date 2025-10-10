#pragma once

#include "smart/geometry.hpp"

#include <stdexcept>
#include <vector>

namespace smart::world {

enum class TerrainType {
    Empty,
    Rock,
    Tree,
    Water,
    AmmoDepot,
    MedicalDepot
};

class GameMap {
public:
    GameMap(int rows, int cols, TerrainType default_tile = TerrainType::Empty);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }

    [[nodiscard]] bool in_bounds(geometry::Position pos) const noexcept;
    [[nodiscard]] TerrainType at(geometry::Position pos) const;
    void set(geometry::Position pos, TerrainType type);

    [[nodiscard]] bool is_passable(geometry::Position pos) const;
    [[nodiscard]] bool blocks_vision(geometry::Position pos) const;
    [[nodiscard]] bool blocks_projectiles(geometry::Position pos) const;

    [[nodiscard]] std::vector<geometry::Position> neighbors4(geometry::Position pos) const;

private:
    int rows_{};
    int cols_{};
    std::vector<TerrainType> tiles_;

    [[nodiscard]] std::size_t index(geometry::Position pos) const;
};

} // namespace smart::world
