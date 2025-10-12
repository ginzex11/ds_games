#include "character.h"

/**
 * @brief Construct a new Character object
 */
Character::Character(Position pos, Team t, CharacterType ct)
    : position(pos), team(t), type(ct), health(INITIAL_HEALTH),
      alive(true), pathIndex(0) {
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
 * @brief Move one step along current path
 */
void Character::moveAlongPath() {
    if (currentPath.empty() || pathIndex >= static_cast<int>(currentPath.size())) {
        return;
    }
    
    position = currentPath[pathIndex];
    pathIndex++;
    
    // Clear path if we've reached the end
    if (pathIndex >= static_cast<int>(currentPath.size())) {
        currentPath.clear();
        pathIndex = 0;
    }
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
