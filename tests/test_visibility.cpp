#include "test_framework.hpp"

#include "smart/game_map.hpp"
#include "smart/visibility.hpp"

using namespace smart;

SMART_TEST_CASE(test_visibility_blocks_by_rocks_and_trees)
{
    world::GameMap map(7, 7, world::TerrainType::Empty);
    map.set({3, 4}, world::TerrainType::Rock);
    map.set({2, 2}, world::TerrainType::Tree);
    map.set({3, 2}, world::TerrainType::Water);

    const auto soldier_view = ai::compute_visibility(map, {3, 3}, /*range=*/3);

    SMART_REQUIRE(soldier_view.contains({3, 2}));
    SMART_REQUIRE(!soldier_view.contains({3, 5}));
    SMART_REQUIRE(!soldier_view.contains({1, 1}));
}

SMART_TEST_CASE(test_commander_combines_team_visibility)
{
    world::GameMap map(7, 7, world::TerrainType::Empty);
    map.set({3, 3}, world::TerrainType::Rock);
    map.set({2, 3}, world::TerrainType::Tree);

    const auto first = ai::compute_visibility(map, {1, 1}, 4);
    const auto second = ai::compute_visibility(map, {5, 5}, 4);

    const auto combined = ai::combine_visibility(map.rows(), map.cols(), {first, second});

    SMART_REQUIRE(combined.contains({1, 1}));
    SMART_REQUIRE(combined.contains({5, 5}));
    SMART_REQUIRE(!combined.contains({3, 3}));
}
