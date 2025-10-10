#include "smart/agent.hpp"

#include <algorithm>

namespace smart {

char Agent::symbol() const
{
    switch (role) {
    case AgentRole::Commander:
        return 'C';
    case AgentRole::Warrior:
        return 'W';
    case AgentRole::Medic:
        return 'M';
    case AgentRole::Supplier:
        return 'P';
    }
    return '?';
}

std::string Agent::name() const
{
    switch (role) {
    case AgentRole::Commander:
        return "Commander";
    case AgentRole::Warrior:
        return "Warrior";
    case AgentRole::Medic:
        return "Medic";
    case AgentRole::Supplier:
        return "Supplier";
    }
    return "Unknown";
}

void Agent::reset_path()
{
    path.clear();
    path_index = 0;
}

bool Agent::advance_step()
{
    if (path.empty() || path_index + 1 >= path.size()) {
        reset_path();
        return false;
    }
    ++path_index;
    position = path[path_index];
    if (path_index + 1 >= path.size()) {
        reset_path();
    }
    return true;
}

} // namespace smart
