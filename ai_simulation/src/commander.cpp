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
    
    // Aggregate team visibility (combines all warrior visibility)
    aggregateTeamVisibility(teamMembers);
    
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
    moveAlongPath(allCharacters);
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
 * @brief Aggregate team visibility from all warriors
 * This implements the assignment requirement: 
 * "Commander's visibility map is built as a combination of the visibility maps of each of the soldiers"
 */
void Commander::aggregateTeamVisibility(const std::vector<Character*>& teamMembers) {
    // Start with commander's own visibility
    teamVisibilityMap = visibleCells;
    
    // Add visibility from all team members (especially warriors)
    for (Character* member : teamMembers) {
        const std::unordered_set<Position>& memberVision = member->getVisibleCells();
        
        // Union: add all positions visible to this team member
        for (const Position& pos : memberVision) {
            teamVisibilityMap.insert(pos);
        }
    }
    
    LOG_CHARACTER("[COMMANDER " << teamToString(team) << "] Team visibility aggregated: " 
                  << teamVisibilityMap.size() << " cells visible (own: " 
                  << visibleCells.size() << ", team boost: " 
                  << (teamVisibilityMap.size() - visibleCells.size()) << ")\n");
}

/**
 * @brief Issue orders to team members
 */
void Commander::issueOrders(const std::vector<Character*>& teamMembers, const Map& map) {
    LOG_CHARACTER("[COMMANDER " << teamToString(team) << "] Issuing orders to team...\n");
    
    // FIRST PASS: Check for critical situations that need immediate response
    // Find retreating warriors that need urgent medical attention
    Warrior* criticalWarrior = nullptr;
    Character* teamMedic = nullptr;
    
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::MEDIC) {
            teamMedic = member;
        }
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getIsRetreating() && !criticalWarrior) {
                criticalWarrior = w;  // Found first retreating warrior
            }
        }
    }
    
    // CRITICAL: If we have a retreating warrior and a medic, assign medic immediately
    if (criticalWarrior && teamMedic) {
        // Check if medic is already healing this specific warrior
        Medic* m = dynamic_cast<Medic*>(teamMedic);
        bool alreadyAssigned = (m && m->getCurrentPatient() == criticalWarrior);
        
        if (!alreadyAssigned) {
            LOG_CHARACTER("  [Commander] CRITICAL OVERRIDE: Reassigning medic to RETREATING warrior at (" 
                     << criticalWarrior->getPosition().x << "," << criticalWarrior->getPosition().y 
                     << ") with HP:" << criticalWarrior->getHealth() << "\n");
            
            Order healOrder = Order(OrderType::HEAL, criticalWarrior->getPosition(), criticalWarrior);
            teamMedic->executeOrder(healOrder, map);
        }
    }
    
    // SECOND PASS: Regular order assignment
    for (Character* member : teamMembers) {
        // Check if warrior is retreating - DO NOT interrupt retreat!
        if (member->getType() == CharacterType::WARRIOR) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getIsRetreating()) {
                LOG_CHARACTER("  - W is RETREATING, skipping (do not interrupt!)\n");
                continue;  // Warriors in retreat mode manage themselves
            }
        }
        
        // Skip medic if we just reassigned them to critical patient
        if (member == teamMedic && criticalWarrior) {
            LOG_CHARACTER("  - M assigned to critical patient, skipping\n");
            continue;
        }
        
        // Only issue new orders if member doesn't have an active order
        if (member->getCurrentOrder().type != OrderType::NONE) {
            LOG_CHARACTER("  - " << characterTypeToChar(member->getType()) 
                     << " already has " << orderTypeToString(member->getCurrentOrder().type) 
                     << " order, skipping\n");
            continue;  // Skip this member - they're already busy
        }
        
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
            LOG_CHARACTER("  - Issuing " << orderTypeToString(order.type) << " order to " 
                     << characterTypeToChar(member->getType()) << " at (" 
                     << member->getPosition().x << "," << member->getPosition().y << ")"
                     << " | Target: (" << order.targetPosition.x << "," << order.targetPosition.y << ")\n");
            member->executeOrder(order, map);
        } else {
            LOG_CHARACTER("  - No order for " << characterTypeToChar(member->getType()) << "\n");
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
    
    // No enemies known - patrol aggressively towards enemy territory
    // Give each warrior a slightly different patrol position to avoid clustering
    Position warriorPos = warrior->getPosition();
    Position patrolTarget;
    
    if (team == Team::BLUE) {
        // Blue team patrols towards right side (orange territory)
        int baseX = GRID_WIDTH * 3 / 4;
        int baseY = GRID_HEIGHT / 2;
        
        // Spread warriors vertically based on their starting Y position
        if (warriorPos.y < GRID_HEIGHT / 2) {
            patrolTarget = Position(baseX, baseY - 3);  // Lower patrol point
        } else {
            patrolTarget = Position(baseX, baseY + 3);  // Upper patrol point
        }
    } else {
        // Orange team patrols towards left side (blue territory)
        int baseX = GRID_WIDTH / 4;
        int baseY = GRID_HEIGHT / 2;
        
        // Spread warriors vertically based on their starting Y position
        if (warriorPos.y < GRID_HEIGHT / 2) {
            patrolTarget = Position(baseX, baseY - 3);  // Lower patrol point
        } else {
            patrolTarget = Position(baseX, baseY + 3);  // Upper patrol point
        }
    }
    
    // Only give patrol order if warrior is not already near the patrol point
    if (warrior->getPosition().euclideanDistance(patrolTarget) > 5) {
        return Order(OrderType::MOVE, patrolTarget);
    }
    
    // Already at patrol position, no order (will engage if enemies appear)
    return Order();
}

/**
 * @brief Determine order for medic
 * PRIORITY SYSTEM:
 * 1. Warriors in retreat mode (HP <= 40%) - CRITICAL
 * 2. Warriors needing healing (HP <= 50%) - HIGH
 */
Order Commander::determineMedicOrder(Character* medic, const std::vector<Character*>& teamMembers, const Map& map) {
    // PRIORITY 1: Find retreating warriors (HP <= 40%) - CRITICAL
    Warrior* criticalWarrior = nullptr;
    float closestCriticalDist = 999999.0f;
    
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getIsRetreating()) {
                float dist = medic->getPosition().euclideanDistance(member->getPosition());
                if (dist < closestCriticalDist) {
                    closestCriticalDist = dist;
                    criticalWarrior = w;
                }
            }
        }
    }
    
    if (criticalWarrior) {
        LOG_CHARACTER("  [Commander] PRIORITY HEAL: Medic assigned to RETREATING " << teamToString(criticalWarrior->getTeam()) 
                 << " warrior at (" << criticalWarrior->getPosition().x << "," << criticalWarrior->getPosition().y 
                 << ") with HP:" << criticalWarrior->getHealth() << " - CRITICAL!\n");
        return Order(OrderType::HEAL, criticalWarrior->getPosition(), criticalWarrior);
    }
    
    // PRIORITY 2: Find warriors needing healing (HP <= 50%) - HIGH
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getNeedsHealing()) {
                LOG_CHARACTER("  [Commander] Assigning medic to heal " << teamToString(member->getTeam()) 
                         << " warrior at (" << member->getPosition().x << "," << member->getPosition().y 
                         << ") with HP:" << member->getHealth() << "\n");
                return Order(OrderType::HEAL, member->getPosition(), member);
            }
        }
    }
    
    // No one needs healing, return no order (medic will stay idle/safe)
    return Order();
}

/**
 * @brief Determine order for supplier
 */
Order Commander::determineSupplierOrder(Character* supplier, const std::vector<Character*>& teamMembers, const Map& map) {
    // Find warriors needing ammo
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {  // Check if alive!
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getNeedsAmmo()) {
                return Order(OrderType::RESUPPLY, member->getPosition(), member);
            }
        }
    }
    
    // No one needs ammo, return no order (supplier will stay idle/safe)
    return Order();
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
