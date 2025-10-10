#pragma once

#include "smart/game_map.hpp"
#include "smart/geometry.hpp"

#include <initializer_list>
#include <vector>

namespace smart::ai {

class VisibilityGrid {
public:
    VisibilityGrid(int rows, int cols);

    void mark(geometry::Position pos);
    [[nodiscard]] bool contains(geometry::Position pos) const;
    void merge(const VisibilityGrid &other);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }

    [[nodiscard]] const std::vector<bool> &raw() const noexcept { return visible_; }

private:
    int rows_{};
    int cols_{};
    std::vector<bool> visible_;
    [[nodiscard]] std::size_t index(geometry::Position pos) const;
};

VisibilityGrid compute_visibility(
    const world::GameMap &map,
    geometry::Position origin,
    int range);

VisibilityGrid combine_visibility(
    int rows,
    int cols,
    std::initializer_list<VisibilityGrid> grids);

VisibilityGrid combine_visibility(
    int rows,
    int cols,
    const std::vector<VisibilityGrid> &grids);

bool has_line_of_sight(
    const world::GameMap &map,
    geometry::Position origin,
    geometry::Position target,
    bool treat_trees_as_blocking = true);

} // namespace smart::ai
