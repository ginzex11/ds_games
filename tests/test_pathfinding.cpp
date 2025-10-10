#include "test_framework.hpp"

#include "smart/game_map.hpp"
#include "smart/pathfinding.hpp"
#include "smart/safety_map.hpp"

using namespace smart;

SMART_TEST_CASE(test_astar_avoids_impassable_and_risky_cells)
{
    world::GameMap map(5, 5, world::TerrainType::Empty);
    map.set({2, 2}, world::TerrainType::Rock);
    map.set({1, 2}, world::TerrainType::Tree);

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({1, 1}, 10.0);
    safety.set({1, 2}, 10.0);
    safety.set({1, 3}, 10.0);

    geometry::Position start{0, 0};
    geometry::Position goal{4, 4};

    const auto path = ai::find_path_a_star(map, safety, start, goal);

    SMART_REQUIRE(path.front() == start);
    SMART_REQUIRE(path.back() == goal);

    for (const auto &tile : path) {
        SMART_REQUIRE(map.is_passable(tile));
        SMART_REQUIRE(safety.risk(tile) < 9.0);
    }
}

SMART_TEST_CASE(test_bfs_finds_nearest_safe_cell_within_radius)
{
    world::GameMap map(5, 5, world::TerrainType::Empty);
    map.set({1, 1}, world::TerrainType::Rock);
    map.set({1, 2}, world::TerrainType::Rock);
    map.set({1, 3}, world::TerrainType::Rock);

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({0, 1}, 15.0);
    safety.set({0, 2}, 20.0);
    safety.set({0, 3}, 15.0);

    geometry::Position origin{0, 0};

    const auto best = ai::find_nearest_safe_cell(map, safety, origin, /*max_radius=*/4, /*max_risk=*/5.0);

    SMART_REQUIRE(best.has_value());
    SMART_REQUIRE(*best != origin);
    SMART_REQUIRE(map.is_passable(*best));
    SMART_REQUIRE(safety.risk(*best) <= 5.0);
}
