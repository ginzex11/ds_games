#include "ai.h"
#include <queue>
#include <limits>

/**
 * @brief A* pathfinding with safety consideration
 */
std::vector<Position> AI::findPath(
    const Position& start,
    const Position& goal,
    const Map& map,
    const std::vector<std::vector<float>>* safetyMap,
    float safetyWeight
) {
    if (!map.isPassable(start) || !map.isPassable(goal)) {
        return {};
    }
    
    if (start == goal) {
        return {start};
    }
    
    // Priority queue for A* (min-heap)
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    std::unordered_set<Position> closedSet;
    std::unordered_map<Position, float> gScores;
    std::unordered_map<Position, Position> cameFrom;
    
    // Initialize
    gScores[start] = 0.0f;
    openSet.push(AStarNode(start, 0.0f, heuristic(start, goal), Position(-1, -1)));
    
    while (!openSet.empty()) {
        AStarNode current = openSet.top();
        openSet.pop();
        
        if (current.pos == goal) {
            return reconstructPath(cameFrom, current.pos);
        }
        
        if (closedSet.count(current.pos)) {
            continue;
        }
        
        closedSet.insert(current.pos);
        
        // Check neighbors
        for (const Position& neighbor : getNeighbors(current.pos)) {
            if (!isValidPosition(neighbor) || !map.isPassable(neighbor)) {
                continue;
            }
            
            if (closedSet.count(neighbor)) {
                continue;
            }
            
            // Calculate cost to neighbor
            float moveCost = 1.0f;
            
            // Add safety cost if safety map provided
            if (safetyMap != nullptr && safetyWeight > 0.0f) {
                float safetyCost = (*safetyMap)[neighbor.y][neighbor.x] * safetyWeight;
                moveCost += safetyCost;
            }
            
            float tentativeG = current.g + moveCost;
            
            // If this path to neighbor is better
            if (gScores.find(neighbor) == gScores.end() || tentativeG < gScores[neighbor]) {
                gScores[neighbor] = tentativeG;
                cameFrom[neighbor] = current.pos;
                float h = heuristic(neighbor, goal);
                openSet.push(AStarNode(neighbor, tentativeG, h, current.pos));
            }
        }
    }
    
    // No path found
    return {};
}

/**
 * @brief Find nearest safe position using BFS
 */
Position AI::findNearestSafePosition(
    const Position& start,
    const Map& map,
    const std::vector<std::vector<float>>& safetyMap,
    float safetyThreshold
) {
    // Check if current position is already safe
    if (safetyMap[start.y][start.x] <= safetyThreshold) {
        return start;
    }
    
    std::queue<Position> queue;
    std::unordered_set<Position> visited;
    
    queue.push(start);
    visited.insert(start);
    
    while (!queue.empty()) {
        Position current = queue.front();
        queue.pop();
        
        // Check if this position is safe
        if (safetyMap[current.y][current.x] <= safetyThreshold && map.isPassable(current)) {
            return current;
        }
        
        // Add neighbors to queue
        for (const Position& neighbor : getNeighbors(current)) {
            if (isValidPosition(neighbor) && !visited.count(neighbor)) {
                visited.insert(neighbor);
                queue.push(neighbor);
            }
        }
    }
    
    // No safe position found, return start
    return start;
}

/**
 * @brief Bresenham's line algorithm for line of sight
 */
bool AI::hasLineOfSight(const Position& from, const Position& to, const Map& map) {
    int x0 = from.x, y0 = from.y;
    int x1 = to.x, y1 = to.y;
    
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        // Don't check the starting position
        if (!(x == x0 && y == y0)) {
            Position current(x, y);
            
            // If we reached the target, we have line of sight
            if (current == to) {
                return true;
            }
            
            // Check if this position blocks sight
            if (!isValidPosition(current) || map.blocksSight(current)) {
                return false;
            }
        }
        
        if (x == x1 && y == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return true;
}

/**
 * @brief Calculate visibility from a position
 */
std::unordered_set<Position> AI::calculateVisibility(
    const Position& from,
    int range,
    const Map& map
) {
    std::unordered_set<Position> visible;
    
    // Check all positions within range
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            Position pos(from.x + dx, from.y + dy);
            
            if (!isValidPosition(pos)) continue;
            
            // Check if within circular range
            float distance = from.euclideanDistance(pos);
            if (distance > range) continue;
            
            // Check line of sight
            if (hasLineOfSight(from, pos, map)) {
                visible.insert(pos);
            }
        }
    }
    
    return visible;
}

/**
 * @brief Generate safety map based on enemy threat
 */
std::vector<std::vector<float>> AI::generateSafetyMap(
    const std::vector<Position>& enemyPositions,
    const Map& map
) {
    std::vector<std::vector<float>> safetyMap(GRID_HEIGHT, std::vector<float>(GRID_WIDTH, 0.0f));
    
    // For each enemy, add threat value to nearby cells
    for (const Position& enemyPos : enemyPositions) {
        // Enemy can threaten up to SHOOT_RANGE distance
        int threatRange = SHOOT_RANGE + 2;
        
        for (int dy = -threatRange; dy <= threatRange; ++dy) {
            for (int dx = -threatRange; dx <= threatRange; ++dx) {
                Position pos(enemyPos.x + dx, enemyPos.y + dy);
                
                if (!isValidPosition(pos)) continue;
                
                float distance = enemyPos.euclideanDistance(pos);
                if (distance > threatRange) continue;
                
                // Check if enemy has line of sight to this position
                if (hasLineOfSight(enemyPos, pos, map)) {
                    // Threat decreases with distance
                    float threat = 10.0f * (1.0f - distance / threatRange);
                    safetyMap[pos.y][pos.x] += threat;
                }
            }
        }
    }
    
    return safetyMap;
}

/**
 * @brief Find optimal attack position
 */
Position AI::findAttackPosition(
    const Position& from,
    const Position& target,
    int range,
    const Map& map
) {
    float currentDistance = from.euclideanDistance(target);
    
    // If already in range and have line of sight, stay here
    if (currentDistance <= range && hasLineOfSight(from, target, map)) {
        return from;
    }
    
    // Find positions in range of target
    std::vector<std::pair<Position, float>> candidates;
    
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            Position pos(target.x + dx, target.y + dy);
            
            if (!isValidPosition(pos) || !map.isPassable(pos)) continue;
            
            float distance = target.euclideanDistance(pos);
            if (distance > range) continue;
            
            // Check if position has line of sight to target
            if (hasLineOfSight(pos, target, map)) {
                // Prefer positions closer to current position
                float distanceFromCurrent = from.euclideanDistance(pos);
                candidates.push_back({pos, distanceFromCurrent});
            }
        }
    }
    
    // Return closest valid position
    if (!candidates.empty()) {
        auto best = std::min_element(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        return best->first;
    }
    
    // No valid position found, return current
    return from;
}

/**
 * @brief Manhattan distance heuristic for A*
 */
float AI::heuristic(const Position& a, const Position& b) {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

/**
 * @brief Get 4-directional neighbors
 */
std::vector<Position> AI::getNeighbors(const Position& pos) {
    return {
        Position(pos.x + 1, pos.y),
        Position(pos.x - 1, pos.y),
        Position(pos.x, pos.y + 1),
        Position(pos.x, pos.y - 1)
    };
}

/**
 * @brief Reconstruct path from A* came-from map
 */
std::vector<Position> AI::reconstructPath(
    const std::unordered_map<Position, Position>& cameFrom,
    Position current
) {
    std::vector<Position> path;
    path.push_back(current);
    
    while (cameFrom.find(current) != cameFrom.end()) {
        current = cameFrom.at(current);
        if (current.x == -1) break;  // Reached start
        path.push_back(current);
    }
    
    std::reverse(path.begin(), path.end());
    return path;
}
