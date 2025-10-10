#include "smart/command.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace smart::ai {

bool MovementPlan::contains(geometry::Position pos) const
{
    return std::find(steps.begin(), steps.end(), pos) != steps.end();
}

geometry::Position MovementPlan::destination() const
{
    if (steps.empty()) {
        return {};
    }
    return steps.back();
}

bool MovementPlan::is_within_range_of(geometry::Position pos) const
{
    if (steps.empty()) {
        return false;
    }
    if (engagement_range <= 0) {
        return destination() == pos;
    }
    return geometry::manhattan_distance(destination(), pos) <= engagement_range;
}

std::optional<MovementPlan> plan_support_route(
    const world::GameMap &map,
    const SafetyMap &safety,
    const SupportRequest &request)
{
    const auto first_leg = find_path_a_star(map, safety, request.start, request.depot);
    if (first_leg.empty()) {
        return std::nullopt;
    }
    const auto second_leg = find_path_a_star(map, safety, request.depot, request.target);
    if (second_leg.empty()) {
        return std::nullopt;
    }

    MovementPlan plan;
    plan.steps = first_leg;
    plan.steps.insert(plan.steps.end(), std::next(second_leg.begin()), second_leg.end());
    plan.engagement_range = 0;
    return plan;
}

std::optional<MovementPlan> plan_attack_route(
    const world::GameMap &map,
    const SafetyMap &safety,
    const AttackRequest &request,
    double max_risk_per_tile)
{
    if (!map.is_passable(request.start)) {
        return std::nullopt;
    }

    const int rows = map.rows();
    const int cols = map.cols();

    double best_cost = std::numeric_limits<double>::infinity();
    MovementPlan best_plan;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const geometry::Position candidate{row, col};
            if (!map.is_passable(candidate)) {
                continue;
            }
            if (geometry::manhattan_distance(candidate, request.target) > request.attack_range) {
                continue;
            }
            if (!has_line_of_sight(map, candidate, request.target, true)) {
                continue;
            }
            if (safety.risk(candidate) > max_risk_per_tile) {
                continue;
            }

            const auto path = find_path_a_star(map, safety, request.start, candidate);
            if (path.empty()) {
                continue;
            }

            double path_cost = 0.0;
            bool safe_path = true;
            for (const auto &step : path) {
                const double risk = safety.risk(step);
                if (risk > max_risk_per_tile) {
                    safe_path = false;
                    break;
                }
                path_cost += risk;
            }

            if (!safe_path) {
                continue;
            }

            if (path_cost < best_cost) {
                best_cost = path_cost;
                best_plan.steps = path;
                best_plan.engagement_range = request.attack_range;
            }
        }
    }

    if (std::isfinite(best_cost)) {
        return best_plan;
    }

    return std::nullopt;
}

} // namespace smart::ai
