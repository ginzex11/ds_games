#include "smart/pathfinding.hpp"

#include "smart/geometry.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

namespace smart::ai {
namespace {

struct Node {
    geometry::Position position;
    double cost;
    double priority;
};

struct NodeCompare {
    bool operator()(const Node &lhs, const Node &rhs) const
    {
        return lhs.priority > rhs.priority;
    }
};

double estimate_cost(geometry::Position a, geometry::Position b)
{
    return static_cast<double>(geometry::manhattan_distance(a, b));
}

std::vector<geometry::Position> reconstruct_path(
    const std::unordered_map<geometry::Position, geometry::Position> &came_from,
    geometry::Position start,
    geometry::Position goal)
{
    std::vector<geometry::Position> path;
    path.push_back(goal);
    auto current = goal;
    while (current != start) {
        current = came_from.at(current);
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace

std::vector<geometry::Position> find_path_a_star(
    const world::GameMap &map,
    const SafetyMap &safety,
    geometry::Position start,
    geometry::Position goal)
{
    if (!map.is_passable(start) || !map.is_passable(goal)) {
        return {};
    }

    if (start == goal) {
        return {start};
    }

    std::priority_queue<Node, std::vector<Node>, NodeCompare> frontier;
    frontier.push(Node{start, 0.0, estimate_cost(start, goal)});

    std::unordered_map<geometry::Position, geometry::Position> came_from;
    std::unordered_map<geometry::Position, double> cost_so_far;
    came_from[start] = start;
    cost_so_far[start] = 0.0;

    while (!frontier.empty()) {
        const auto current = frontier.top();
        frontier.pop();

        if (current.position == goal) {
            return reconstruct_path(came_from, start, goal);
        }

        for (const auto &next : map.neighbors4(current.position)) {
            if (!map.is_passable(next)) {
                continue;
            }
            const double risk = safety.risk(next);
            const double new_cost = cost_so_far[current.position] + 1.0 + risk;
            if (!cost_so_far.contains(next) || new_cost < cost_so_far[next]) {
                cost_so_far[next] = new_cost;
                const double priority = new_cost + estimate_cost(next, goal);
                frontier.push(Node{next, new_cost, priority});
                came_from[next] = current.position;
            }
        }
    }

    return {};
}

std::optional<geometry::Position> find_nearest_safe_cell(
    const world::GameMap &map,
    const SafetyMap &safety,
    geometry::Position origin,
    int max_radius,
    double max_risk)
{
    if (max_radius < 0) {
        return std::nullopt;
    }

    std::queue<geometry::Position> frontier;
    std::unordered_map<geometry::Position, int> distance;

    frontier.push(origin);
    distance[origin] = 0;

    std::optional<geometry::Position> best;
    double best_risk = std::numeric_limits<double>::infinity();

    while (!frontier.empty()) {
        const auto current = frontier.front();
        frontier.pop();

        const int dist = distance[current];
        if (dist > max_radius) {
            continue;
        }

        if (current != origin && map.is_passable(current)) {
            const double risk = safety.risk(current);
            if (risk <= max_risk && risk < best_risk) {
                best = current;
                best_risk = risk;
            }
        }

        for (const auto &next : map.neighbors4(current)) {
            if (distance.contains(next)) {
                continue;
            }
            distance[next] = dist + 1;
            frontier.push(next);
        }
    }

    return best;
}

} // namespace smart::ai
