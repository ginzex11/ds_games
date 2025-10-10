#include "smart/simulation.hpp"

#include "smart/pathfinding.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace smart {
namespace {
constexpr int COMMANDER_VIS_RANGE = 8;
constexpr int WARRIOR_VIS_RANGE = 6;
constexpr int SUPPORT_VIS_RANGE = 5;

constexpr double MAX_ATTACK_RISK = 10.0;

} // namespace

Agent &TeamState::commander()
{
    auto it = std::find_if(agents.begin(), agents.end(), [](const Agent &agent) {
        return agent.role == AgentRole::Commander;
    });
    if (it == agents.end()) {
        throw std::runtime_error("Commander missing from team state");
    }
    return *it;
}

const Agent &TeamState::commander() const
{
    return const_cast<TeamState *>(this)->commander();
}

std::vector<Agent *> TeamState::warriors()
{
    std::vector<Agent *> result;
    for (auto &agent : agents) {
        if (agent.role == AgentRole::Warrior) {
            result.push_back(&agent);
        }
    }
    return result;
}

std::vector<const Agent *> TeamState::warriors() const
{
    std::vector<const Agent *> result;
    for (const auto &agent : agents) {
        if (agent.role == AgentRole::Warrior) {
            result.push_back(&agent);
        }
    }
    return result;
}

std::vector<Agent *> TeamState::supports()
{
    std::vector<Agent *> result;
    for (auto &agent : agents) {
        if (is_support_role(agent.role)) {
            result.push_back(&agent);
        }
    }
    return result;
}

Simulation::Simulation(int rows, int cols)
    : map_(rows, cols, world::TerrainType::Empty)
    , safety_(rows, cols, 1.0)
    , rng_(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
{
    reset();
}

void Simulation::reset()
{
    tick_ = 0;
    paused_ = false;
    initialize_map();
    initialize_teams();
    rebuild_agent_index();
    recompute_safety_map();
}

void Simulation::initialize_map()
{
    const int rows = map_.rows();
    const int cols = map_.cols();

    // Clear map to empty
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            map_.set({r, c}, world::TerrainType::Empty);
        }
    }

    // Place some obstacles and terrain features
    for (int c = 6; c < cols - 6; ++c) {
        map_.set({rows / 2, c}, world::TerrainType::Water);
    }

    for (int r = 3; r < rows - 3; ++r) {
        map_.set({r, cols / 3}, world::TerrainType::Tree);
        map_.set({r, 2 * cols / 3}, world::TerrainType::Tree);
    }

    for (int c = 4; c < cols - 4; c += 2) {
        map_.set({rows / 3, c}, world::TerrainType::Rock);
        map_.set({2 * rows / 3, c + 1}, world::TerrainType::Rock);
    }

    blue_.ammo_depot = {rows - 3, 1};
    blue_.medical_depot = {rows - 3, 3};
    map_.set(blue_.ammo_depot, world::TerrainType::AmmoDepot);
    map_.set(blue_.medical_depot, world::TerrainType::MedicalDepot);

    orange_.ammo_depot = {1, cols - 3};
    orange_.medical_depot = {1, cols - 1};
    map_.set(orange_.ammo_depot, world::TerrainType::AmmoDepot);
    map_.set(orange_.medical_depot, world::TerrainType::MedicalDepot);
}

void Simulation::initialize_teams()
{
    blue_.id = TeamId::Blue;
    orange_.id = TeamId::Orange;

    blue_.agents.clear();
    orange_.agents.clear();

    auto make_agent = [](int id, TeamId team, AgentRole role, geometry::Position pos) {
        Agent agent;
        agent.id = id;
        agent.team = team;
        agent.role = role;
        agent.position = pos;
        agent.objective = pos;
        if (role == AgentRole::Commander) {
            agent.max_health = agent.health = 120;
        } else if (role == AgentRole::Warrior) {
            agent.max_health = agent.health = 100;
            agent.max_ammo = agent.ammo = 40;
            agent.grenades = 3;
        } else {
            agent.max_health = agent.health = 80;
            agent.max_ammo = agent.ammo = 20;
        }
        return agent;
    };

    const geometry::Position base_blue{map_.rows() - 2, 1};
    const geometry::Position base_orange{1, map_.cols() - 2};

    blue_.agents.push_back(make_agent(0, TeamId::Blue, AgentRole::Commander, {base_blue.row, base_blue.col}));
    blue_.agents.push_back(make_agent(1, TeamId::Blue, AgentRole::Warrior, {base_blue.row, base_blue.col + 2}));
    blue_.agents.push_back(make_agent(2, TeamId::Blue, AgentRole::Warrior, {base_blue.row - 2, base_blue.col + 2}));
    blue_.agents.push_back(make_agent(3, TeamId::Blue, AgentRole::Medic, {base_blue.row + 1, base_blue.col}));
    blue_.agents.push_back(make_agent(4, TeamId::Blue, AgentRole::Supplier, {base_blue.row + 1, base_blue.col + 2}));

    orange_.agents.push_back(make_agent(5, TeamId::Orange, AgentRole::Commander, {base_orange.row, base_orange.col}));
    orange_.agents.push_back(make_agent(6, TeamId::Orange, AgentRole::Warrior, {base_orange.row, base_orange.col - 2}));
    orange_.agents.push_back(make_agent(7, TeamId::Orange, AgentRole::Warrior, {base_orange.row + 2, base_orange.col - 2}));
    orange_.agents.push_back(make_agent(8, TeamId::Orange, AgentRole::Medic, {base_orange.row - 1, base_orange.col}));
    orange_.agents.push_back(make_agent(9, TeamId::Orange, AgentRole::Supplier, {base_orange.row - 1, base_orange.col - 2}));

    blue_support_ = {};
    orange_support_ = {};
}

void Simulation::rebuild_agent_index()
{
    agents_flat_.clear();
    for (const auto &agent : blue_.agents) {
        agents_flat_.push_back(agent);
    }
    for (const auto &agent : orange_.agents) {
        agents_flat_.push_back(agent);
    }
}

void Simulation::recompute_safety_map()
{
    const int rows = map_.rows();
    const int cols = map_.cols();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            double base = 1.0;
            const geometry::Position pos{r, c};

            if (!map_.is_passable(pos)) {
                safety_.set(pos, 100.0);
                continue;
            }

            const int dist_to_center = geometry::manhattan_distance(pos, {rows / 2, cols / 2});
            base += 3.0 / (static_cast<double>(dist_to_center) + 1.0);

            const auto adjust_risk = [&](const TeamState &opponents) {
                for (const auto &agent : opponents.agents) {
                    if (!agent.is_alive()) {
                        continue;
                    }
                    const int d = geometry::manhattan_distance(pos, agent.position);
                    base += 6.0 / (static_cast<double>(d) + 2.0);
                }
            };

            adjust_risk(blue_);
            adjust_risk(orange_);
            safety_.set(pos, base);
        }
    }
}

void Simulation::toggle_pause()
{
    paused_ = !paused_;
}

void Simulation::step_once()
{
    update();
}

void Simulation::update()
{
    if (paused_) {
        return;
    }

    ++tick_;

    degrade_resources(blue_);
    degrade_resources(orange_);

    recompute_safety_map();

    update_team(blue_, orange_, blue_support_);
    update_team(orange_, blue_, orange_support_);

    resolve_engagements(blue_, orange_);
    resolve_engagements(orange_, blue_);

    advance_agents(blue_);
    advance_agents(orange_);

    rebuild_agent_index();
}

void Simulation::degrade_resources(TeamState &team)
{
    if (tick_ % 10 != 0) {
        return;
    }
    for (auto &agent : team.agents) {
        if (!agent.is_alive()) {
            continue;
        }
        if (agent.role == AgentRole::Warrior && agent.ammo > 0) {
            --agent.ammo;
        }
        if (agent.role == AgentRole::Warrior && tick_ % 30 == 0 && agent.health > agent.max_health / 4) {
            agent.health -= 2;
        }
    }
}

void Simulation::update_team(TeamState &team, TeamState &enemy, SupportMission &mission)
{
    issue_warrior_orders(team, enemy);
    handle_support(team, mission);
}

void Simulation::handle_support(TeamState &team, SupportMission &mission)
{
    const auto needs_ammo = find_low_ammo_warrior(team);
    const auto needs_medic = find_injured_warrior(team);

    if (mission.active) {
        if (mission.supporter_index < 0 || mission.supporter_index >= static_cast<int>(team.agents.size())) {
            mission.active = false;
        } else {
            auto &supporter = team.agents.at(mission.supporter_index);
            if (supporter.path.empty()) {
                mission.active = false;
                if (mission.beneficiary_index >= 0 && mission.beneficiary_index < static_cast<int>(team.agents.size())) {
                    auto &beneficiary = team.agents[mission.beneficiary_index];
                        if (mission.role == ai::SupportRole::Medic) {
                        beneficiary.health = beneficiary.max_health;
                        } else if (mission.role == ai::SupportRole::Supplier) {
                        beneficiary.ammo = beneficiary.max_ammo;
                    }
                }
            }
        }
        return;
    }

    if (needs_medic.has_value()) {
        int medic_index = -1;
        for (std::size_t i = 0; i < team.agents.size(); ++i) {
            auto &agent = team.agents[i];
            if (agent.role == AgentRole::Medic && agent.path.empty()) {
                medic_index = static_cast<int>(i);
                break;
            }
        }
        if (medic_index >= 0) {
            Agent &medic = team.agents[medic_index];
                ai::SupportRequest request{
                    .role = ai::SupportRole::Medic,
                .start = medic.position,
                .depot = team.medical_depot,
                .target = team.agents[*needs_medic].position,
            };
                if (auto plan = ai::plan_support_route(map_, safety_, request)) {
                medic.path = plan->steps;
                medic.path_index = 0;
                mission.supporter_index = medic_index;
                mission.beneficiary_index = *needs_medic;
                    mission.role = ai::SupportRole::Medic;
                mission.depot = team.medical_depot;
                mission.active = true;
                return;
            }
        }
    }

    if (needs_ammo.has_value()) {
        int supplier_index = -1;
        for (std::size_t i = 0; i < team.agents.size(); ++i) {
            auto &agent = team.agents[i];
            if (agent.role == AgentRole::Supplier && agent.path.empty()) {
                supplier_index = static_cast<int>(i);
                break;
            }
        }
        if (supplier_index >= 0) {
            Agent &supplier = team.agents[supplier_index];
                ai::SupportRequest request{
                    .role = ai::SupportRole::Supplier,
                .start = supplier.position,
                .depot = team.ammo_depot,
                .target = team.agents[*needs_ammo].position,
            };
                if (auto plan = ai::plan_support_route(map_, safety_, request)) {
                supplier.path = plan->steps;
                supplier.path_index = 0;
                mission.supporter_index = supplier_index;
                mission.beneficiary_index = *needs_ammo;
                    mission.role = ai::SupportRole::Supplier;
                mission.depot = team.ammo_depot;
                mission.active = true;
                return;
            }
        }
    }
}

void Simulation::issue_warrior_orders(TeamState &team, TeamState &enemy)
{
    std::vector<const Agent *> visible_enemies;
    for (const auto &agent : enemy.agents) {
        if (!agent.is_alive()) {
            continue;
        }
        visible_enemies.push_back(&agent);
    }

    for (auto *warrior : team.warriors()) {
        if (!warrior->is_alive()) {
            continue;
        }
        if (!warrior->path.empty()) {
            continue;
        }
        const Agent *target = nullptr;
        int best_score = std::numeric_limits<int>::max();
        for (const auto *enemy_agent : visible_enemies) {
            const int dist = geometry::manhattan_distance(warrior->position, enemy_agent->position);
            if (dist < best_score) {
                best_score = dist;
                target = enemy_agent;
            }
        }
        if (!target) {
            continue;
        }

        ai::AttackRequest request{
            .start = warrior->position,
            .target = target->position,
            .attack_range = 4,
        };

            if (auto plan = ai::plan_attack_route(map_, safety_, request, MAX_ATTACK_RISK)) {
            warrior->path = plan->steps;
            warrior->path_index = 0;
            warrior->task = AgentTask::Advance;
        } else if (auto fallback = ai::find_nearest_safe_cell(map_, safety_, warrior->position, 6, MAX_ATTACK_RISK)) {
            auto defensive_path = ai::find_path_a_star(map_, safety_, warrior->position, *fallback);
            warrior->path = defensive_path;
            warrior->path_index = 0;
            warrior->task = AgentTask::Defend;
        }
    }
}

void Simulation::advance_agents(TeamState &team)
{
    for (auto &agent : team.agents) {
        if (!agent.is_alive() || agent.path.empty()) {
            continue;
        }

        // Check if the next position is available
        const std::size_t next_index = agent.path_index + 1;
        if (next_index >= agent.path.size()) {
            agent.reset_path();
            agent.task = AgentTask::Idle;
            continue;
        }

        const geometry::Position next_pos = agent.path[next_index];
        
        // Check if position is occupied by another agent
        bool position_occupied = false;
        for (const auto &other_team : {std::ref(blue_), std::ref(orange_)}) {
            for (const auto &other_agent : other_team.get().agents) {
                if (other_agent.is_alive() && other_agent.position == next_pos) {
                    position_occupied = true;
                    break;
                }
            }
            if (position_occupied) break;
        }

        if (!position_occupied) {
            agent.advance_step();
        }

        if (agent.path.empty()) {
            agent.task = AgentTask::Idle;
        }
    }
}

void Simulation::resolve_engagements(TeamState &team, TeamState &enemy)
{
    for (auto &warrior : team.agents) {
        if (warrior.role != AgentRole::Warrior || !warrior.is_alive()) {
            continue;
        }
        if (warrior.ammo <= 0) {
            continue;
        }
        for (auto &opponent : enemy.agents) {
            if (!opponent.is_alive()) {
                continue;
            }
            const int dist = geometry::manhattan_distance(warrior.position, opponent.position);
                if (dist <= 4 && ai::has_line_of_sight(map_, warrior.position, opponent.position, true)) {
                opponent.health -= 5;
                if (opponent.health <= 0) {
                    opponent.active = false;
                }
                if (tick_ % 3 == 0) {
                    warrior.ammo = std::max(0, warrior.ammo - 1);
                }
            }
        }
    }
}

std::optional<int> Simulation::find_low_ammo_warrior(const TeamState &team) const
{
    for (std::size_t i = 0; i < team.agents.size(); ++i) {
        const auto &agent = team.agents[i];
        if (agent.role == AgentRole::Warrior && agent.is_alive() && agent.needs_ammo()) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

std::optional<int> Simulation::find_injured_warrior(const TeamState &team) const
{
    for (std::size_t i = 0; i < team.agents.size(); ++i) {
        const auto &agent = team.agents[i];
        if (agent.role == AgentRole::Warrior && agent.is_alive() && agent.needs_medical()) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

ai::VisibilityGrid Simulation::commander_visibility(TeamId id) const
{
    const TeamState &team_ref = (id == TeamId::Blue) ? blue_ : orange_;
    std::vector<ai::VisibilityGrid> grids;
    grids.reserve(team_ref.agents.size());
    for (const auto &agent : team_ref.agents) {
        if (!agent.is_alive()) {
            continue;
        }
        int range = (agent.role == AgentRole::Commander) ? COMMANDER_VIS_RANGE
            : (agent.role == AgentRole::Warrior)           ? WARRIOR_VIS_RANGE
                                                           : SUPPORT_VIS_RANGE;
        grids.push_back(ai::compute_visibility(map_, agent.position, range));
    }
    if (grids.empty()) {
        return ai::VisibilityGrid(map_.rows(), map_.cols());
    }
    return ai::combine_visibility(map_.rows(), map_.cols(), grids);
}

const TeamState &Simulation::team(TeamId id) const
{
    return id == TeamId::Blue ? blue_ : orange_;
}

TeamState &Simulation::team(TeamId id)
{
    return id == TeamId::Blue ? blue_ : orange_;
}

} // namespace smart
