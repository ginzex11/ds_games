#pragma once

#include "smart/game_map.hpp"
#include "smart/geometry.hpp"
#include "smart/pathfinding.hpp"
#include "smart/safety_map.hpp"
#include "smart/visibility.hpp"

#include <optional>
#include <vector>

namespace smart::ai {

enum class SupportRole {
    Medic,
    Supplier
};

struct SupportRequest {
    SupportRole role{};
    geometry::Position start{};
    geometry::Position depot{};
    geometry::Position target{};
};

struct AttackRequest {
    geometry::Position start{};
    geometry::Position target{};
    int attack_range{};
};

struct MovementPlan {
    std::vector<geometry::Position> steps;
    int engagement_range{0};

    [[nodiscard]] bool empty() const noexcept { return steps.empty(); }
    [[nodiscard]] bool contains(geometry::Position pos) const;
    [[nodiscard]] bool is_within_range_of(geometry::Position pos) const;
    [[nodiscard]] geometry::Position destination() const;
};

std::optional<MovementPlan> plan_support_route(
    const world::GameMap &map,
    const SafetyMap &safety,
    const SupportRequest &request);

std::optional<MovementPlan> plan_attack_route(
    const world::GameMap &map,
    const SafetyMap &safety,
    const AttackRequest &request,
    double max_risk_per_tile = 10.0);

} // namespace smart::ai
