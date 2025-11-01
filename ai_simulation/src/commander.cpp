#include "commander.h"
#include "warrior.h"
#include "medic.h"
#include "supplier.h"
#include <algorithm>  // For std::sort

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
    issueOrders(teamMembers, map, currentTurn);
    
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
void Commander::issueOrders(const std::vector<Character*>& teamMembers, const Map& map, int currentTurn) {
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
                order = determineWarriorOrder(member, map, currentTurn);
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
Order Commander::determineWarriorOrder(Character* warrior, const Map& map, int currentTurn) {
    Warrior* w = dynamic_cast<Warrior*>(warrior);
    if (!w) return Order();
    
    // FIX 3: Even if warrior needs resources, still attack if enemies visible and has ammo!
    // Don't let warriors sit idle with ammo while enemies nearby
    bool hasAmmo = w->getAmmo() > 0;
    bool hasGrenades = w->getGrenades() > 0;
    bool canFight = hasAmmo || hasGrenades;
    
    // If warrior needs resources BUT can fight, check for enemies first
    if ((w->getNeedsHealing() || w->getNeedsAmmo()) && !canFight) {
        // Only defend if truly unable to fight
        return Order(OrderType::DEFEND);
    }
    
    // If enemies known, attack nearest
    if (!combinedEnemyMap.empty()) {
        // Build sorted list of enemies by distance
        std::vector<std::pair<Position, float>> enemiesByDistance;
        for (const auto& pair : combinedEnemyMap) {
            float dist = warrior->getPosition().euclideanDistance(pair.first);
            enemiesByDistance.push_back({pair.first, dist});
        }
        
        // Sort by distance
        std::sort(enemiesByDistance.begin(), enemiesByDistance.end(),
                 [](const auto& a, const auto& b) { return a.second < b.second; });
        
        // IMPROVED: Try nearest enemy first, but if failed, try next nearest
        // This provides fallback targets instead of giving up completely
        int attemptedTargets = 0;
        int skippedTargets = 0;
        
        for (const auto& enemyPair : enemiesByDistance) {
            Position enemyPos = enemyPair.first;
            attemptedTargets++;
            
            // Skip if warrior has recently failed to reach this target
            if (w->hasFailedDestination(enemyPos)) {
                skippedTargets++;
                LOG_CHARACTER("  - W has previously failed to reach target at (" 
                         << enemyPos.x << "," << enemyPos.y << "), trying next target (" 
                         << skippedTargets << "/" << enemiesByDistance.size() << " skipped)\n");
                continue;  // Try next enemy instead of giving up
            }
            
            // FIX C: Validate target position still has recent enemy sighting (<3 turns)
            // This prevents issuing ATTACK orders to stale positions where enemy has moved/died
            bool targetHasRecentEnemy = false;
            if (combinedEnemyMap.find(enemyPos) != combinedEnemyMap.end()) {
                const EnemySighting& sighting = combinedEnemyMap[enemyPos];
                int turnsSinceSeen = currentTurn - sighting.turnSeen;
                if (turnsSinceSeen < 3) {
                    targetHasRecentEnemy = true;
                } else {
                    LOG_CHARACTER("  - FIX C: Skipping stale target at (" << enemyPos.x << "," << enemyPos.y 
                             << ") - seen " << turnsSinceSeen << " turns ago\n");
                }
            }
            
            if (!targetHasRecentEnemy) {
                skippedTargets++;
                continue;  // Skip stale target
            }
            
            // Found valid target
            if (skippedTargets > 0) {
                LOG_CHARACTER("  - FIX 4: Issuing ATTACK to fallback target #" << attemptedTargets 
                         << " after skipping " << skippedTargets << " failed targets\n");
            }
            return Order(OrderType::ATTACK, enemyPos);
        }
        
        // All known enemies have failed paths - let warrior handle autonomously
        LOG_CHARACTER("  - FIX 4: All " << enemiesByDistance.size() 
                 << " known enemy positions failed previously, warrior will handle autonomously\n");
        return Order();  // Let warrior make independent tactical decision
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
    
    // FIX 3: Always give patrol order to keep warriors moving and scanning
    // Even if near patrol point, rotate to different patrol positions
    return Order(OrderType::MOVE, patrolTarget);
}

/**
 * @brief Determine order for medic
 * PRIORITY SYSTEM:
 * 1. Warriors in retreat mode (HP <= 40%) - CRITICAL
 * 2. Commander if injured (HP <= 50%) - HIGH
 * 3. Warriors needing healing (HP <= 50%) - HIGH
 */
Order Commander::determineMedicOrder(Character* medic, const std::vector<Character*>& teamMembers, const Map& map) {
    // PRIORITY 1: Find retreating warriors (HP <= 40%) - CRITICAL
    // FIX: Prioritize LOWEST HP warrior, not closest distance!
    Warrior* criticalWarrior = nullptr;
    int lowestHP = 999999;
    
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getIsRetreating()) {
                int hp = w->getHealth();
                if (hp < lowestHP) {
                    lowestHP = hp;
                    criticalWarrior = w;
                }
            }
        }
    }
    
    if (criticalWarrior) {
        LOG_CHARACTER("  [Commander] PRIORITY HEAL: Medic assigned to RETREATING " << teamToString(criticalWarrior->getTeam()) 
                 << " warrior at (" << criticalWarrior->getPosition().x << "," << criticalWarrior->getPosition().y 
                 << ") with HP:" << criticalWarrior->getHealth() << " (LOWEST HP) - CRITICAL!\n");
        return Order(OrderType::HEAL, criticalWarrior->getPosition(), criticalWarrior);
    }
    
    // OPTION C PRIORITY 1.5: Find warriors with HP < 40 even if not retreating - URGENT
    // FIX: Prioritize LOWEST HP warrior, not closest distance!
    Warrior* urgentWarrior = nullptr;
    int lowestUrgentHP = 999999;
    
    for (Character* member : teamMembers) {
        if (member->getType() == CharacterType::WARRIOR && member->isAlive()) {
            Warrior* w = dynamic_cast<Warrior*>(member);
            if (w && w->getHealth() < 40) {
                int hp = w->getHealth();
                if (hp < lowestUrgentHP) {
                    lowestUrgentHP = hp;
                    urgentWarrior = w;
                }
            }
        }
    }
    
    if (urgentWarrior) {
        LOG_CHARACTER("  [Commander] OPTION C: Medic assigned to LOW HP warrior (HP < 40) at (" 
                 << urgentWarrior->getPosition().x << "," << urgentWarrior->getPosition().y 
                 << ") with HP:" << urgentWarrior->getHealth() << " (LOWEST HP) - URGENT!\n");
        return Order(OrderType::HEAL, urgentWarrior->getPosition(), urgentWarrior);
    }
    
    // PRIORITY 2: Check if COMMANDER needs healing (HP <= 50%) - HIGH
    if (health <= 50) {
        LOG_CHARACTER("  [Commander] SELF-HEAL: Assigning medic to heal COMMANDER at (" 
                 << position.x << "," << position.y << ") with HP:" << health << "\n");
        return Order(OrderType::HEAL, position, this);
    }
    
    // PRIORITY 3: Find warriors needing healing (HP <= 50%) - HIGH
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
 * Uses depth-limited BFS with defined search range
 */
void Commander::relocateIfNeeded(const Map& map) {
    float currentSafety = teamSafetyMap[position.y][position.x];
    
    // If in danger (safety > 5), find safer position within 20 tiles
    if (currentSafety > 5.0f) {
        Position safePos = AI::findNearestSafePosition(position, map, teamSafetyMap, 2.0f, 20);
        
        if (safePos != position) {
            currentPath = AI::findPath(position, safePos, map, &teamSafetyMap, 1.0f);
            pathIndex = 0;
        }
    }
}
