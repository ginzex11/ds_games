#pragma once

#include "smart/geometry.hpp"

#include <string>
#include <vector>

namespace smart {

enum class TeamId {
    Blue,
    Orange
};

enum class AgentRole {
    Commander,
    Warrior,
    Medic,
    Supplier
};

enum class AgentTask {
    Idle,
    Advance,
    Support,
    Defend
};

struct Agent {
    int id{};
    TeamId team{TeamId::Blue};
    AgentRole role{AgentRole::Warrior};
    AgentTask task{AgentTask::Idle};

    geometry::Position position{};
    geometry::Position objective{};

    std::vector<geometry::Position> path;
    std::size_t path_index{0};

    int health{100};
    int max_health{100};
    int ammo{30};
    int max_ammo{30};
    int grenades{3};

    int cooldown_ticks{0};
    bool active{true};

    [[nodiscard]] char symbol() const;
    [[nodiscard]] std::string name() const;
    [[nodiscard]] bool is_alive() const noexcept { return active && health > 0; }
    [[nodiscard]] bool needs_medical() const noexcept { return health < max_health * 3 / 4; }
    [[nodiscard]] bool needs_ammo() const noexcept { return ammo < max_ammo / 3; }

    void reset_path();
    bool advance_step();
};

[[nodiscard]] inline bool is_support_role(AgentRole role)
{
    return role == AgentRole::Medic || role == AgentRole::Supplier;
}

} // namespace smart
