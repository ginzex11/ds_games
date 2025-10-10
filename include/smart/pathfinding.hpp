#pragma once

#include "smart/game_map.hpp"
#include "smart/geometry.hpp"
#include "smart/safety_map.hpp"

#include <optional>
#include <vector>

namespace smart::ai {

std::vector<geometry::Position> find_path_a_star(
    const world::GameMap &map,
    const SafetyMap &safety,
    geometry::Position start,
    geometry::Position goal);

std::optional<geometry::Position> find_nearest_safe_cell(
    const world::GameMap &map,
    const SafetyMap &safety,
    geometry::Position origin,
    int max_radius,
    double max_risk);

} // namespace smart::ai
