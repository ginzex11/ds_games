#include "character.h"
#include <random>
#include <algorithm>


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
    
    // CRITICAL: Validate position is within map bounds
    if (!isValidPosition(nextPos)) {
        std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                 << "] ERROR: Path contains out-of-bounds position (" 
                 << nextPos.x << "," << nextPos.y << ")! Clearing invalid path.\n";
        currentPath.clear();
        pathIndex = 0;
        blockedTurns = 0;
        return;
    }
    
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
        
        // If blocked for 5+ turns, try to sidestep to break deadlock
        if (blockedTurns >= 5) {
            if (blocker) {
                std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                         << "] Blocked for " << blockedTurns << " turns by " 
                         << characterTypeToChar(blocker->getType()) << " " << teamToString(blocker->getTeam())
                         << " at (" << nextPos.x << "," << nextPos.y << "), trying to sidestep\n";
            }
            
            // Try to move to an adjacent cell to break deadlock
            // RANDOMIZE order to prevent predictable patterns
            std::vector<Position> sideSteps = {
                Position(position.x + 1, position.y),
                Position(position.x - 1, position.y),
                Position(position.x, position.y + 1),
                Position(position.x, position.y - 1),
                Position(position.x + 1, position.y + 1),
                Position(position.x - 1, position.y - 1),
                Position(position.x + 1, position.y - 1),
                Position(position.x - 1, position.y + 1)
            };
            
            // Shuffle the sidestep directions randomly
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(sideSteps.begin(), sideSteps.end(), g);
            
            bool sideStepped = false;
            for (const Position& sideStep : sideSteps) {
                if (!isValidPosition(sideStep)) continue;
                
                // Check if passable and not occupied
                bool canSideStep = true;
                for (const Character* other : allCharacters) {
                    if (other != this && other->isAlive() && other->getPosition() == sideStep) {
                        canSideStep = false;
                        break;
                    }
                }
                
                if (canSideStep) {
                    position = sideStep;
                    sideStepped = true;
                    std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                             << "] Sidestepped to (" << sideStep.x << "," << sideStep.y << ") to break deadlock\n";
                    break;
                }
            }
            
            // Clear path regardless of whether sidestep succeeded
            currentPath.clear();
            pathIndex = 0;
            blockedTurns = 0;
            
            if (!sideStepped) {
                std::cout << "[" << characterTypeToChar(type) << " " << teamToString(team) 
                         << "] Could not sidestep, waiting for blocker to move\n";
            }
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
