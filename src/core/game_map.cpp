#include "smart/game_map.hpp"

#include <array>

namespace smart::world {

namespace {
bool is_passable(TerrainType type)
{
    switch (type) {
    case TerrainType::Empty:
    case TerrainType::Tree:
    case TerrainType::AmmoDepot:
    case TerrainType::MedicalDepot:
        return true;
    case TerrainType::Rock:
    case TerrainType::Water:
        return false;
    }
    return false;
}

bool blocks_vision(TerrainType type)
{
    switch (type) {
    case TerrainType::Rock:
    case TerrainType::Tree:
        return true;
    case TerrainType::Empty:
    case TerrainType::Water:
    case TerrainType::AmmoDepot:
    case TerrainType::MedicalDepot:
        return false;
    }
    return false;
}

bool blocks_projectile(TerrainType type)
{
    switch (type) {
    case TerrainType::Rock:
    case TerrainType::Tree:
        return true;
    case TerrainType::Empty:
    case TerrainType::Water:
    case TerrainType::AmmoDepot:
    case TerrainType::MedicalDepot:
        return false;
    }
    return false;
}
} // namespace

GameMap::GameMap(int rows, int cols, TerrainType default_tile)
    : rows_(rows)
    , cols_(cols)
    , tiles_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), default_tile)
{
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("Map dimensions must be positive");
    }
}

bool GameMap::in_bounds(geometry::Position pos) const noexcept
{
    return pos.row >= 0 && pos.row < rows_ && pos.col >= 0 && pos.col < cols_;
}

TerrainType GameMap::at(geometry::Position pos) const
{
    if (!in_bounds(pos)) {
        throw std::out_of_range("Position is outside of the map bounds");
    }
    return tiles_[index(pos)];
}

void GameMap::set(geometry::Position pos, TerrainType type)
{
    if (!in_bounds(pos)) {
        throw std::out_of_range("Position is outside of the map bounds");
    }
    tiles_[index(pos)] = type;
}

bool GameMap::is_passable(geometry::Position pos) const
{
    if (!in_bounds(pos)) {
        return false;
    }
    return ::smart::world::is_passable(at(pos));
}

bool GameMap::blocks_vision(geometry::Position pos) const
{
    if (!in_bounds(pos)) {
        return true;
    }
    return ::smart::world::blocks_vision(at(pos));
}

bool GameMap::blocks_projectiles(geometry::Position pos) const
{
    if (!in_bounds(pos)) {
        return true;
    }
    return ::smart::world::blocks_projectile(at(pos));
}

std::vector<geometry::Position> GameMap::neighbors4(geometry::Position pos) const
{
    static constexpr std::array<geometry::Position, 4> directions{{
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    }};

    std::vector<geometry::Position> result;
    result.reserve(4);
    for (const auto &dir : directions) {
        const auto candidate = geometry::Position{pos.row + dir.row, pos.col + dir.col};
        if (in_bounds(candidate)) {
            result.push_back(candidate);
        }
    }
    return result;
}

std::size_t GameMap::index(geometry::Position pos) const
{
    return static_cast<std::size_t>(pos.row) * static_cast<std::size_t>(cols_) +
        static_cast<std::size_t>(pos.col);
}

} // namespace smart::world
