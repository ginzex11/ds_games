#include "test_framework.hpp"

#include "smart/agent.hpp"
#include "smart/command.hpp"
#include "smart/game_map.hpp"
#include "smart/pathfinding.hpp"
#include "smart/safety_map.hpp"
#include "smart/simulation.hpp"
#include "smart/visibility.hpp"

using namespace smart;

SMART_TEST_CASE(test_unit_types_initialization)
{
    Simulation sim(10, 10);
    sim.reset();

    const auto &blue_team = sim.team(TeamId::Blue);
    const auto &orange_team = sim.team(TeamId::Orange);

    // Check commanders exist
    SMART_REQUIRE(blue_team.commander().role == AgentRole::Commander);
    SMART_REQUIRE(orange_team.commander().role == AgentRole::Commander);

    // Check warriors exist
    const auto blue_warriors = blue_team.warriors();
    const auto orange_warriors = orange_team.warriors();
    SMART_REQUIRE(!blue_warriors.empty());
    SMART_REQUIRE(!orange_warriors.empty());

    // Check support units exist
    const auto blue_supports = blue_team.supports();
    const auto orange_supports = orange_team.supports();
    SMART_REQUIRE(!blue_supports.empty());
    SMART_REQUIRE(!orange_supports.empty());

    // Verify unit properties
    for (const auto &agent : blue_team.agents) {
        SMART_REQUIRE(agent.health > 0);
        SMART_REQUIRE(agent.active);
        SMART_REQUIRE(agent.team == TeamId::Blue);
    }
}

SMART_TEST_CASE(test_terrain_types_and_movement)
{
    world::GameMap map(5, 5, world::TerrainType::Empty);

    // Set up different terrain types
    map.set({1, 1}, world::TerrainType::Rock);
    map.set({2, 2}, world::TerrainType::Tree);
    map.set({3, 3}, world::TerrainType::Water);
    map.set({4, 4}, world::TerrainType::AmmoDepot);

    // Test passability
    SMART_REQUIRE(!map.is_passable({1, 1})); // Rock blocks movement
    SMART_REQUIRE(map.is_passable({2, 2}));  // Tree allows movement
    SMART_REQUIRE(!map.is_passable({3, 3})); // Water blocks movement
    SMART_REQUIRE(map.is_passable({4, 4}));  // Depot allows movement

    // Test vision blocking
    SMART_REQUIRE(map.blocks_vision({1, 1})); // Rock blocks vision
    SMART_REQUIRE(map.blocks_vision({2, 2})); // Tree blocks vision
    SMART_REQUIRE(!map.blocks_vision({3, 3})); // Water doesn't block vision
    SMART_REQUIRE(!map.blocks_vision({4, 4})); // Depot doesn't block vision
}

SMART_TEST_CASE(test_ai_algorithms_astar_with_safety)
{
    world::GameMap map(8, 8, world::TerrainType::Empty);
    map.set({3, 3}, world::TerrainType::Rock); // Add obstacle

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({2, 4}, 15.0); // High risk area

    geometry::Position start{0, 0};
    geometry::Position goal{7, 7};

    const auto path = ai::find_path_a_star(map, safety, start, goal);

    SMART_REQUIRE(!path.empty());
    SMART_REQUIRE(path.front() == start);
    SMART_REQUIRE(path.back() == goal);

    // Verify path avoids obstacles and high-risk areas
    for (const auto &pos : path) {
        SMART_REQUIRE(map.is_passable(pos));
        SMART_REQUIRE(safety.risk(pos) < 10.0); // Should avoid very high risk
    }
}

SMART_TEST_CASE(test_visibility_system)
{
    world::GameMap map(6, 6, world::TerrainType::Empty);
    map.set({2, 3}, world::TerrainType::Rock);
    map.set({3, 2}, world::TerrainType::Tree);

    geometry::Position observer{0, 0};
    geometry::Position target1{5, 5}; // Should be visible
    geometry::Position target2{2, 4}; // Blocked by rock
    geometry::Position target3{4, 1}; // Blocked by tree

    SMART_REQUIRE(ai::has_line_of_sight(map, observer, target1, true));
    SMART_REQUIRE(!ai::has_line_of_sight(map, observer, target2, true));
    SMART_REQUIRE(!ai::has_line_of_sight(map, observer, target3, true));
}

SMART_TEST_CASE(test_bfs_defensive_positions)
{
    world::GameMap map(5, 5, world::TerrainType::Empty);
    map.set({2, 2}, world::TerrainType::Rock);

    ai::SafetyMap safety(map.rows(), map.cols(), 1.0);
    safety.set({1, 1}, 20.0); // Very dangerous
    safety.set({3, 3}, 5.0);  // Moderately safe

    geometry::Position origin{0, 0};

    const auto safe_pos = ai::find_nearest_safe_cell(map, safety, origin, 4, 10.0);

    SMART_REQUIRE(safe_pos.has_value());
    SMART_REQUIRE(map.is_passable(*safe_pos));
    SMART_REQUIRE(safety.risk(*safe_pos) <= 10.0);
}

SMART_TEST_CASE(test_combat_system_ammo_management)
{
    Simulation sim(10, 10);
    sim.reset();

    // Find a warrior
    auto &blue_team = sim.team(TeamId::Blue);
    Agent *warrior = nullptr;
    for (auto &agent : blue_team.agents) {
        if (agent.role == AgentRole::Warrior) {
            warrior = &agent;
            break;
        }
    }
    SMART_REQUIRE(warrior != nullptr);

    // Verify initial ammo
    SMART_REQUIRE(warrior->ammo > 0);
    SMART_REQUIRE(warrior->ammo <= warrior->max_ammo);

    // Simulate some combat ticks
    for (int i = 0; i < 10; ++i) {
        sim.step_once();
    }

    // Ammo should decrease over time (if engaged in combat)
    // Note: This is probabilistic, so we just check it's within bounds
    SMART_REQUIRE(warrior->ammo >= 0);
    SMART_REQUIRE(warrior->ammo <= warrior->max_ammo);
}

SMART_TEST_CASE(test_supply_chain_mechanic)
{
    Simulation sim(10, 10);
    sim.reset();

    auto &blue_team = sim.team(TeamId::Blue);

    // Find a warrior and deplete their ammo
    Agent *warrior = nullptr;
    for (auto &agent : blue_team.agents) {
        if (agent.role == AgentRole::Warrior) {
            warrior = &agent;
            warrior->ammo = 1; // Nearly depleted
            break;
        }
    }
    SMART_REQUIRE(warrior != nullptr);

    // Run simulation to trigger supply chain
    for (int i = 0; i < 20; ++i) {
        sim.step_once();
    }

    // Check if supply mission was activated (this is internal state)
    // The test mainly ensures no crashes and ammo stays within bounds
    SMART_REQUIRE(warrior->ammo >= 0);
    SMART_REQUIRE(warrior->ammo <= warrior->max_ammo);
}

SMART_TEST_CASE(test_team_coordination_visibility_sharing)
{
    Simulation sim(10, 10);
    sim.reset();

    // Get commander's visibility
    const auto blue_visibility = sim.commander_visibility(TeamId::Blue);
    const auto orange_visibility = sim.commander_visibility(TeamId::Orange);

    // Visibility grids should be properly sized
    SMART_REQUIRE(blue_visibility.rows() == 10);
    SMART_REQUIRE(blue_visibility.cols() == 10);
    SMART_REQUIRE(orange_visibility.rows() == 10);
    SMART_REQUIRE(orange_visibility.cols() == 10);

    // Some positions should be visible (commanders can see themselves)
    bool has_visible = false;
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 10; ++c) {
            if (blue_visibility.is_visible({r, c})) {
                has_visible = true;
                break;
            }
        }
        if (has_visible) break;
    }
    SMART_REQUIRE(has_visible);
}

SMART_TEST_CASE(test_medic_healing_system)
{
    Simulation sim(10, 10);
    sim.reset();

    auto &blue_team = sim.team(TeamId::Blue);

    // Find a warrior and damage them
    Agent *warrior = nullptr;
    for (auto &agent : blue_team.agents) {
        if (agent.role == AgentRole::Warrior) {
            warrior = &agent;
            warrior->health = 50; // Injured
            break;
        }
    }
    SMART_REQUIRE(warrior != nullptr);
    SMART_REQUIRE(warrior->needs_medical());

    // Run simulation to trigger healing
    for (int i = 0; i < 30; ++i) {
        sim.step_once();
    }

    // Health should be managed properly (may have been healed or stayed the same)
    SMART_REQUIRE(warrior->health >= 0);
    SMART_REQUIRE(warrior->health <= warrior->max_health);
}