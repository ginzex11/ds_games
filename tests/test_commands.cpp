#include "test_framework.hpp"

#include "smart/command.hpp"
#include "smart/game_map.hpp"
#include "smart/pathfinding.hpp"
#include "smart/safety_map.hpp"
#include "smart/visibility.hpp"

using namespace smart;

SMART_TEST_CASE(test_medic_route_visits_medical_depot)
{
    world::GameMap map(6, 6, world::TerrainType::Empty);
    map.set({0, 3}, world::TerrainType::MedicalDepot);

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({2, 2}, 8.0);
    safety.set({2, 3}, 8.0);

    ai::SupportRequest request{
        .role = ai::SupportRole::Medic,
        .start = {5, 0},
        .depot = {0, 3},
        .target = {5, 5},
    };

    const auto plan = ai::plan_support_route(map, safety, request);

    SMART_REQUIRE(plan.has_value());
    SMART_REQUIRE_EQ(plan->steps.front().row, request.start.row);
    SMART_REQUIRE_EQ(plan->steps.front().col, request.start.col);
    SMART_REQUIRE(plan->contains(request.depot));
    SMART_REQUIRE(plan->contains(request.target));
}

SMART_TEST_CASE(test_warrior_attack_plan_finds_vantage_point)
{
    world::GameMap map(8, 8, world::TerrainType::Empty);
    map.set({3, 4}, world::TerrainType::Rock);
    map.set({4, 4}, world::TerrainType::Tree);

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({2, 5}, 12.0);

    const geometry::Position warrior{7, 0};
    const geometry::Position enemy{2, 6};

    const auto plan = ai::plan_attack_route(map, safety, {warrior, enemy, /*attack_range=*/4});

    SMART_REQUIRE(plan.has_value());
    SMART_REQUIRE(plan->is_within_range_of(enemy));

    for (const auto &step : plan->steps) {
        SMART_REQUIRE(map.is_passable(step));
        SMART_REQUIRE(safety.risk(step) <= 10.0);
    }
}
