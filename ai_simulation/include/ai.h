#ifndef AI_H
#define AI_H

#include "common.h"
#include "map.h"

// Forward declarations
class Character;

/**
 * @brief AI module providing pathfinding, visibility, and tactical calculations
 * 
 * Contains algorithms for:
 * - A* pathfinding with safety considerations
 * - BFS for finding nearest safe positions
 * - Line-of-sight calculations
 * - Visibility map generation
 * - Safety map generation based on enemy positions
 */
class AI {
public:
    /**
     * @brief Find path using A* algorithm with optional safety consideration
     * 
     * @param start Starting position
     * @param goal Goal position
     * @param map Reference to the game map
     * @param safetyMap Optional safety scores for each cell (higher = more dangerous)
     * @param safetyWeight Weight to apply to safety scores (0.0 = ignore safety)
     * @return std::vector<Position> Path from start to goal (empty if no path found)
     */
    static std::vector<Position> findPath(
        const Position& start,
        const Position& goal,
        const Map& map,
        const std::vector<std::vector<float>>* safetyMap = nullptr,
        float safetyWeight = 0.0f
    );
    
    /**
     * @brief Find nearest safe position using depth-limited BFS
     * 
     * @param start Starting position
     * @param map Reference to the game map
     * @param safetyMap Safety scores for each cell
     * @param safetyThreshold Maximum acceptable safety score
     * @param maxDistance Maximum search distance (depth limit for BFS)
     * @return Position Nearest safe position (returns start if none found)
     */
    static Position findNearestSafePosition(
        const Position& start,
        const Map& map,
        const std::vector<std::vector<float>>& safetyMap,
        float safetyThreshold,
        int maxDistance = 15  // Default search range as required by assignment
    );
    
    /**
     * @brief Check if there's line of sight between two positions
     * 
     * @param from Starting position
     * @param to Target position
     * @param map Reference to the game map
     * @return true if line of sight exists
     */
    static bool hasLineOfSight(const Position& from, const Position& to, const Map& map);
    
    /**
     * @brief Calculate visibility map from a position
     * 
     * @param from Observer position
     * @param range Maximum visibility range
     * @param map Reference to the game map
     * @return std::unordered_set<Position> Set of visible positions
     */
    static std::unordered_set<Position> calculateVisibility(
        const Position& from,
        int range,
        const Map& map
    );
    
    /**
     * @brief Generate safety map based on enemy positions and threat levels
     * 
     * @param enemyPositions Positions of enemy characters
     * @param map Reference to the game map
     * @return std::vector<std::vector<float>> Safety scores (0 = safe, higher = more dangerous)
     */
    static std::vector<std::vector<float>> generateSafetyMap(
        const std::vector<Position>& enemyPositions,
        const Map& map
    );
    
    /**
     * @brief Find position in range for attack
     * 
     * @param from Attacker position
     * @param target Target position
     * @param range Attack range
     * @param map Reference to the game map
     * @return Position Best position to attack from (may be current position)
     */
    static Position findAttackPosition(
        const Position& from,
        const Position& target,
        int range,
        const Map& map
    );

private:
    // A* helper structures
    struct AStarNode {
        Position pos;
        float g;  // Cost from start
        float h;  // Heuristic to goal
        float f;  // Total cost (g + h)
        Position parent;
        
        AStarNode(Position p, float gCost, float hCost, Position par)
            : pos(p), g(gCost), h(hCost), f(gCost + hCost), parent(par) {}
        
        bool operator>(const AStarNode& other) const {
            return f > other.f;
        }
    };
    
    // Helper functions
    static float heuristic(const Position& a, const Position& b);
    static std::vector<Position> getNeighbors(const Position& pos);
    static std::vector<Position> reconstructPath(
        const std::unordered_map<Position, Position>& cameFrom,
        Position current
    );
};

#endif // AI_H
