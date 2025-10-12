#ifndef CHARACTER_H
#define CHARACTER_H

#include "common.h"
#include "map.h"
#include "ai.h"

/**
 * @brief Base class for all character types
 * 
 * Provides common functionality:
 * - Position and movement
 * - Health management
 * - Visibility and perception
 * - Enemy reporting
 * - Path following
 */
class Character {
protected:
    Position position;
    Team team;
    CharacterType type;
    int health;
    bool alive;
    std::vector<Position> currentPath;
    int pathIndex;
    int blockedTurns;  // Count how many turns we've been blocked
    std::unordered_set<Position> visibleCells;
    std::vector<EnemySighting> enemySightings;
    Order currentOrder;
    
public:
    Character(Position pos, Team t, CharacterType ct);
    virtual ~Character() = default;
    
    // Pure virtual - must be implemented by derived classes
    virtual void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) = 0;
    virtual void executeOrder(Order order, const Map& map) = 0;
    
    // Common functionality
    void updateVisibility(const Map& map);
    void scanForEnemies(const std::vector<Character*>& allCharacters, int currentTurn);
    void moveAlongPath(const std::vector<Character*>& allCharacters);
    void takeDamage(int damage);
    
    // Getters
    Position getPosition() const { return position; }
    Team getTeam() const { return team; }
    CharacterType getType() const { return type; }
    int getHealth() const { return health; }
    bool isAlive() const { return alive; }
    const std::unordered_set<Position>& getVisibleCells() const { return visibleCells; }
    const std::vector<EnemySighting>& getEnemySightings() const { return enemySightings; }
    Order getCurrentOrder() const { return currentOrder; }
    char getTypeChar() const { return characterTypeToChar(type); }
    
    // Setters
    void setPosition(Position pos) { position = pos; }
    void setHealth(int h) { health = h; if (health <= 0) { health = 0; alive = false; } }
    
    // Utility
    bool canSee(const Position& targetPos) const {
        return visibleCells.count(targetPos) > 0;
    }
    
    Character* findNearestEnemy(const std::vector<Character*>& allCharacters) const;
};

#endif // CHARACTER_H
