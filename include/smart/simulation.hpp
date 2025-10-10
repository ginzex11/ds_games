#pragma once

#include "smart/agent.hpp"
#include "smart/command.hpp"
#include "smart/game_map.hpp"
#include "smart/safety_map.hpp"
#include "smart/visibility.hpp"

#include <array>
#include <optional>
#include <random>
#include <vector>

namespace smart {

struct TeamState {
    TeamId id{TeamId::Blue};
    geometry::Position ammo_depot{};
    geometry::Position medical_depot{};
    std::vector<Agent> agents;

    [[nodiscard]] Agent &commander();
    [[nodiscard]] const Agent &commander() const;
    [[nodiscard]] std::vector<Agent *> warriors();
    [[nodiscard]] std::vector<const Agent *> warriors() const;
    [[nodiscard]] std::vector<Agent *> supports();
};

struct SupportMission {
    int supporter_index{-1};
    int beneficiary_index{-1};
    ai::SupportRole role{ai::SupportRole::Medic};
    geometry::Position depot{};
    bool active{false};
};

class Simulation {
public:
    Simulation(int rows, int cols);

    void reset();
    void update();
    void toggle_pause();
    void step_once();

    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] const world::GameMap &map() const noexcept { return map_; }
    [[nodiscard]] const ai::SafetyMap &safety_map() const noexcept { return safety_; }
    [[nodiscard]] const TeamState &team(TeamId id) const;
    [[nodiscard]] TeamState &team(TeamId id);
    [[nodiscard]] const std::vector<Agent> &all_agents() const { return agents_flat_; }

    ai::VisibilityGrid commander_visibility(TeamId id) const;

private:
    world::GameMap map_;
    ai::SafetyMap safety_;

    TeamState blue_;
    TeamState orange_;

    std::vector<Agent> agents_flat_;

    int tick_{0};
    bool paused_{false};

    std::mt19937 rng_;

    SupportMission blue_support_{};
    SupportMission orange_support_{};

    void initialize_map();
    void initialize_teams();
    void rebuild_agent_index();
    void recompute_safety_map();

    void update_team(TeamState &team, TeamState &enemy, SupportMission &mission);
    void handle_support(TeamState &team, SupportMission &mission);
    void issue_warrior_orders(TeamState &team, TeamState &enemy);
    void advance_agents(TeamState &team);
    void resolve_engagements(TeamState &team, TeamState &enemy);
    void degrade_resources(TeamState &team);

    std::optional<int> find_low_ammo_warrior(const TeamState &team) const;
    std::optional<int> find_injured_warrior(const TeamState &team) const;
};

} // namespace smart
