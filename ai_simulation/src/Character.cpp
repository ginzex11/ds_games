#include "character.h"

/**
 * @brief Construct a new Character object
 */
Character::Character(Position pos, Team t, CharacterType ct)
    : position(pos), team(t), type(ct), health(INITIAL_HEALTH),
      alive(true), pathIndex(0), blockedTurns(0) {
    currentOrder = Order();
}

/**
 * @brief Update character's visibility range
 */
void Character::updateVisibility(const Map& map) {
    visibleCells = AI::calculateVisibility(position, VISIBILITY_RANGE, map);
}

/**
 * @brief Scan for enemies within visible range and report them
 */
void Character::scanForEnemies(const std::vector<Character*>& allCharacters, int currentTurn) {
    enemySightings.clear();
    
    for (Character* other : allCharacters) {
        if (!other->isAlive() || other->getTeam() == team) {
            continue;
        }
        
        Position enemyPos = other->getPosition();
        if (canSee(enemyPos)) {
            enemySightings.push_back(EnemySighting(enemyPos, other->getType(), currentTurn));
        }
    }
}

/**
 * @brief Move one step along current path (with collision detection)
 */
void Character::moveAlongPath(const std::vector<Character*>& allCharacters) {
    if (currentPath.empty() || pathIndex >= static_cast<int>(currentPath.size())) {
        blockedTurns = 0;
        return;
    }
    
    Position nextPos = currentPath[pathIndex];
    
    // Check if next position is occupied by another character
    bool occupied = false;
    Character* blocker = nullptr;
    for (const Character* other : allCharacters) {
        if (other != this && other->isAlive() && other->getPosition() == nextPos) {
            occupied = true;
            blocker = const_cast<Character*>(other);
            break;
        }
    }
    
    // Only move if position is not occupied
    if (!occupied) {
        position = nextPos;
        pathIndex++;
        blockedTurns = 0;  // Reset block counter when we successfully move
        
        // Clear path if we've reached the end
        if (pathIndex >= static_cast<int>(currentPath.size())) {
            currentPath.clear();
            pathIndex = 0;
        }
    } else {
        // Path is blocked - increment counter
        blockedTurns++;
        
        // If blocked for 3+ turns, clear path so it will be recalculated
        if (blockedTurns >= 3) {
            if (blocker) {
                std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                         << "] Blocked for " << blockedTurns << " turns by " 
                         << characterTypeToChar(blocker->getType()) << " " << teamToString(blocker->getTeam())
                         << " at (" << nextPos.x << "," << nextPos.y << "), clearing path\n";
            } else {
                std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                         << "] Blocked for " << blockedTurns << " turns, clearing path\n";
            }
            currentPath.clear();
            pathIndex = 0;
            blockedTurns = 0;
        }
    }
    // If occupied, wait (don't advance pathIndex, try again next turn)
}

/**
 * @brief Apply damage to character
 */
void Character::takeDamage(int damage) {
    health -= damage;
    if (health <= 0) {
        health = 0;
        alive = false;
    }
}

/**
 * @brief Find nearest visible enemy
 */
Character* Character::findNearestEnemy(const std::vector<Character*>& allCharacters) const {
    Character* nearest = nullptr;
    float minDistance = std::numeric_limits<float>::max();
    
    for (Character* other : allCharacters) {
        if (!other->isAlive() || other->getTeam() == team) {
            continue;
        }
        
        Position enemyPos = other->getPosition();
        if (canSee(enemyPos)) {
            float distance = position.euclideanDistance(enemyPos);
            if (distance < minDistance) {
                minDistance = distance;
                nearest = other;
            }
        }
    }
    
    return nearest;
}
