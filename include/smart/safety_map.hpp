#pragma once

#include "smart/geometry.hpp"

#include <vector>

namespace smart::ai {

class SafetyMap {
public:
    SafetyMap(int rows, int cols, double default_risk = 1.0);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }

    [[nodiscard]] double risk(geometry::Position pos) const;
    void set(geometry::Position pos, double value);

    [[nodiscard]] bool in_bounds(geometry::Position pos) const noexcept;

private:
    int rows_{};
    int cols_{};
    std::vector<double> risk_;   
    [[nodiscard]] std::size_t index(geometry::Position pos) const;
};

} // namespace smart::ai
