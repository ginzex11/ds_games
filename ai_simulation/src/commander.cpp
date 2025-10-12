#include "commander.h"
#include "warrior.h"
#include "medic.h"
#include "supplier.h"

/**
 * @brief Construct a new Commander
 */
Commander::Commander(Position pos, Team t)
    : Character(pos, t, CharacterType::COMMANDER) {
}

/**
 * @brief Update commander - coordinate team and issue orders
 */
void Commander::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Update visibility
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
    // Get team members
    std::vector<Character*> teamMembers;
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() == team && c != this) {
            teamMembers.push_back(c);
        }
    }
    
    // Build combined visibility from team reports
    buildCombinedVisibilityMap(teamMembers, currentTurn);
    
    // Generate safety map based on known enemies
    std::vector<Position> enemyPositions;
    for (const auto& pair : combinedEnemyMap) {
        enemyPositions.push_back(pair.first);
    }
    teamSafetyMap = AI::generateSafetyMap(enemyPositions, map);
    
    // Issue orders to team
    issueOrders(teamMembers, map);
    
    // Relocate if in danger
    relocateIfNeeded(map);
    
    // Move along current path
    moveAlongPath();
}

/**
 * @brief Execute order (commanders typically don't receive orders)
 */
void Commander::executeOrder(Order order, const Map& map) {
    currentOrder = order;
    
    if (order.type == OrderType::MOVE) {
        currentPath = AI::findPath(position, order.targetPosition, map, &teamSafetyMap, 0.5f);
        pathIndex = 0;
    }
}

/**
 * @brief Build combined enemy map from team sightings
 */
void Commander::buildCombinedVisibilityMap(const std::vector<Character*>& teamMembers, int currentTurn) {
    // Add commander's own sightings
    for (const EnemySighting& sighting : enemySightings) {
        combinedEnemyMap[sighting.position] = sighting;
    }
    
    // Add sightings from team members
    for (Character* member : teamMembers) {
        for (const EnemySighting& sighting : member->getEnemySightings()) {
            // Update if this is a newer sighting
            if (combinedEnemyMap.find(sighting.position) == combinedEnemyMap.end() ||
                combinedEnemyMap[sighting.position].turnSeen < sighting.turnSeen) {
                combinedEnemyMap[sighting.position] = sighting;
            }
        }
    }
    
    // Remove old sightings (older than 5 turns)
    auto it = combinedEnemyMap.begin();
    while (it != combinedEnemyMap.end()) {
        if (currentTurn - it->second.turnSeen > 5) {
            it = combinedEnemyMap.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Issue orders to team members
 */
void Commander::issueOrders(const std::vector<Character*>& teamMembers, const Map& map) {
    for (Character* member : teamMembers) {
        Order order;
        
        switch (member->getType()) {
            case CharacterType::WARRIOR:
                order = determineWarriorOrder(member, map);
                break;
            case CharacterType::MEDIC:
                order = determineMedicOrder(member, teamMembers, map);
                break;
            case CharacterType::SUPPLIER:
                order = determineSupplierOrder(member, teamMembers, map);
                break;
            default:
                break;
        }
        
        if (order.type != OrderType::NONE) {
            member->executeOrder(order, map);
        }
    }
}

/**
 * @brief Determine order for warrior
 */
Order Commander::determineWarriorOrder(Character* warrior, const Map& map) {
    Warrior* w = dynamic_cast<Warrior*>(warrior);
    if (!w) return Order();
    
    // If warrior needs resources, let medic/supplier handle it
    if (w->getNeedsHealing() || w->getNeedsAmmo()) {
        return Order(OrderType::DEFEND);
    }
    
    // If enemies known, attack nearest
    if (!combinedEnemyMap.empty()) {
        // Find nearest enemy
        Position nearestEnemy = combinedEnemyMap.begin()->first;
        float minDist = warrior->getPosition().euclideanDistance(nearestEnemy);
        
        for (const auto& pair : combinedEnemyMap) {
            float dist = warrior->getPosition().euclideanDistance(pair.first);
            if (dist < minDist) {
                minDist = dist;
                nearestEnemy = pair.first;
            }
        }
        
        return Order(OrderType::ATTACK, nearestEnemy);
    }
    
    // No enemies, patrol towards center
    Position center(GRID_WIDTH / 2, GRID_HEIGHT / 2);
    return Order(OrderType::MOVE, center);
}

/**
 * @brief Determine order for medic
 */
Order Commander::determineMedicOrder(Character* medic, const std::vector<Character*>& teamMembers, const Map& map) {
    // Find warriors needing healing
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getNeedsHealing()) {
                return Order(OrderType::HEAL, member->getPosition(), member);
            }
        }
    }
    
    // No one needs healing, stay near commander
    return Order(OrderType::MOVE, position);
}

/**
 * @brief Determine order for supplier
 */
Order Commander::determineSupplierOrder(Character* supplier, const std::vector<Character*>& teamMembers, const Map& map) {
    // Find warriors needing ammo
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getNeedsAmmo()) {
                return Order(OrderType::RESUPPLY, member->getPosition(), member);
            }
        }
    }
    
    // No one needs ammo, stay near commander
    return Order(OrderType::MOVE, position);
}

/**
 * @brief Relocate commander to safer position if threatened
 */
void Commander::relocateIfNeeded(const Map& map) {
    float currentSafety = teamSafetyMap[position.y][position.x];
    
    // If in danger (safety > 5), find safer position
    if (currentSafety > 5.0f) {
        Position safePos = AI::findNearestSafePosition(position, map, teamSafetyMap, 2.0f);
        
        if (safePos != position) {
            currentPath = AI::findPath(position, safePos, map, &teamSafetyMap, 1.0f);
            pathIndex = 0;
        }
    }
}
