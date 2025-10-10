#include "smart/visibility.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace smart::ai {

VisibilityGrid::VisibilityGrid(int rows, int cols)
    : rows_(rows)
    , cols_(cols)
    , visible_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), false)
{
}

void VisibilityGrid::mark(geometry::Position pos)
{
    if (pos.row < 0 || pos.row >= rows_ || pos.col < 0 || pos.col >= cols_) {
        return;
    }
    visible_[index(pos)] = true;
}

bool VisibilityGrid::contains(geometry::Position pos) const
{
    if (pos.row < 0 || pos.row >= rows_ || pos.col < 0 || pos.col >= cols_) {
        return false;
    }
    return visible_[index(pos)];
}

void VisibilityGrid::merge(const VisibilityGrid &other)
{
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("Cannot merge visibility grids of different sizes");
    }
    for (std::size_t i = 0; i < visible_.size(); ++i) {
        visible_[i] = visible_[i] || other.visible_[i];
    }
}

std::size_t VisibilityGrid::index(geometry::Position pos) const
{
    return static_cast<std::size_t>(pos.row) * static_cast<std::size_t>(cols_) +
        static_cast<std::size_t>(pos.col);
}

namespace {

bool terrain_blocks(const world::GameMap &map, geometry::Position pos, bool treat_trees_as_blocking)
{
    const auto terrain = map.at(pos);
    switch (terrain) {
    case world::TerrainType::Rock:
        return true;
    case world::TerrainType::Tree:
        return treat_trees_as_blocking;
    case world::TerrainType::Water:
    case world::TerrainType::Empty:
    case world::TerrainType::AmmoDepot:
    case world::TerrainType::MedicalDepot:
        return false;
    }
    return false;
}

} // namespace

bool has_line_of_sight(
    const world::GameMap &map,
    geometry::Position origin,
    geometry::Position target,
    bool treat_trees_as_blocking)
{
    if (!map.in_bounds(origin) || !map.in_bounds(target)) {
        return false;
    }

    auto x0 = origin.col;
    auto y0 = origin.row;
    const auto x1 = target.col;
    const auto y1 = target.row;

    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);

    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (true) {
        const geometry::Position current{y0, x0};
        if (current != origin && current != target) {
            if (terrain_blocks(map, current, treat_trees_as_blocking)) {
                return false;
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    if (terrain_blocks(map, target, treat_trees_as_blocking)) {
        return false;
    }

    return true;
}

VisibilityGrid compute_visibility(
    const world::GameMap &map,
    geometry::Position origin,
    int range)
{
    VisibilityGrid grid(map.rows(), map.cols());
    if (!map.in_bounds(origin)) {
        return grid;
    }

    grid.mark(origin);
    const int max_range = std::max(0, range);

    for (int row = origin.row - max_range; row <= origin.row + max_range; ++row) {
        for (int col = origin.col - max_range; col <= origin.col + max_range; ++col) {
            const geometry::Position candidate{row, col};
            if (!map.in_bounds(candidate)) {
                continue;
            }
            if (geometry::chebyshev_distance(origin, candidate) > max_range) {
                continue;
            }
            if (has_line_of_sight(map, origin, candidate, true)) {
                grid.mark(candidate);
            }
        }
    }

    return grid;
}

VisibilityGrid combine_visibility(
    int rows,
    int cols,
    std::initializer_list<VisibilityGrid> grids)
{
    VisibilityGrid combined(rows, cols);
    for (const auto &grid : grids) {
        combined.merge(grid);
    }
    return combined;
}

VisibilityGrid combine_visibility(
    int rows,
    int cols,
    const std::vector<VisibilityGrid> &grids)
{
    VisibilityGrid combined(rows, cols);
    for (const auto &grid : grids) {
        combined.merge(grid);
    }
    return combined;
}

} // namespace smart::ai
