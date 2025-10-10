#include "smart/safety_map.hpp"

#include <stdexcept>

namespace smart::ai {

SafetyMap::SafetyMap(int rows, int cols, double default_risk)
    : rows_(rows)
    , cols_(cols)
    , risk_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), default_risk)
{
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("Safety map dimensions must be positive");
    }
}

bool SafetyMap::in_bounds(geometry::Position pos) const noexcept
{
    return pos.row >= 0 && pos.row < rows_ && pos.col >= 0 && pos.col < cols_;
}

double SafetyMap::risk(geometry::Position pos) const
{
    if (!in_bounds(pos)) {
        throw std::out_of_range("Position is outside of safety map bounds");
    }
    return risk_[index(pos)];
}

void SafetyMap::set(geometry::Position pos, double value)
{
    if (!in_bounds(pos)) {
        throw std::out_of_range("Position is outside of safety map bounds");
    }
    risk_[index(pos)] = value;
}

std::size_t SafetyMap::index(geometry::Position pos) const
{
    return static_cast<std::size_t>(pos.row) * static_cast<std::size_t>(cols_) +
        static_cast<std::size_t>(pos.col);
}

} // namespace smart::ai
