#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <ostream>

namespace smart::geometry {

struct Position {
    int row{};
    int col{};

    constexpr bool operator==(const Position &other) const noexcept
    {
        return row == other.row && col == other.col;
    }

    constexpr bool operator!=(const Position &other) const noexcept
    {
        return !(*this == other);
    }

    constexpr Position operator+(const Position &other) const noexcept
    {
        return {row + other.row, col + other.col};
    }

    constexpr Position operator-(const Position &other) const noexcept
    {
        return {row - other.row, col - other.col};
    }
};

inline std::ostream &operator<<(std::ostream &os, const Position &pos)
{
    return os << "(" << pos.row << ", " << pos.col << ")";
}

inline int manhattan_distance(const Position &a, const Position &b)
{
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

inline int chebyshev_distance(const Position &a, const Position &b)
{
    return std::max(std::abs(a.row - b.row), std::abs(a.col - b.col));
}

} // namespace smart::geometry

namespace std {

template<> struct hash<smart::geometry::Position> {
    std::size_t operator()(const smart::geometry::Position &pos) const noexcept
    {
        return (static_cast<std::size_t>(pos.row) << 32) ^ static_cast<std::size_t>(pos.col);
    }
};

} // namespace std
