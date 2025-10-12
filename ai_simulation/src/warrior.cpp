#include "warrior.h"

/**
 * @brief Construct a new Warrior
 */
Warrior::Warrior(Position pos, Team t)
    : Character(pos, t, CharacterType::WARRIOR),
      ammo(INITIAL_AMMO), grenades(INITIAL_GRENADES),
      needsAmmo(false), needsHealing(false), isRetreating(false), 
      retreatTarget(pos), lastLoggedTurn(-1) {
}

/**
 * @brief Update warrior - execute orders or act autonomously
 */
void Warrior::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Update visibility and scan
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
    // Check resource status
    checkResources();
    
    // PRIORITY 1: Evaluate retreat conditions (40% HP or lower)
    evaluateRetreat(allCharacters);
    
    // Log status once per turn (each warrior tracks its own last log turn)
    if (currentTurn != lastLoggedTurn) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << " at (" << position.x << "," << position.y 
                 << ")] HP:" << health << "/" << INITIAL_HEALTH 
                 << " | Ammo:" << ammo << "/" << INITIAL_AMMO
                 << " | Order:" << orderTypeToString(currentOrder.type));
        if (isRetreating) LOG_CHARACTER(" | RETREATING!");
        if (needsHealing) LOG_CHARACTER(" | NEEDS HEALING");
        if (needsAmmo) LOG_CHARACTER(" | NEEDS AMMO");
        LOG_CHARACTER("\n");
        lastLoggedTurn = currentTurn;
    }
    
    // PRIORITY 2: If retreating, execute retreat (overrides all other behavior)
    if (isRetreating) {
        executeRetreat(map, allCharacters);
        return;  // Retreat is top priority - exit early
    }
    
    // PRIORITY 3: If critically low on resources but not retreating, defensive mode
    if (needsHealing || needsAmmo) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Low on resources, defensive mode\n");
        
        // Still shoot at ANY visible enemy (defensive mode doesn't mean passive!)
        Character* visibleEnemy = findNearestEnemy(allCharacters);
        if (visibleEnemy) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Defensive shot at enemy\n");
            tryShootEnemy(visibleEnemy, map);
        }
        
        // If we have a DEFEND order, execute it and stay put
        if (currentOrder.type == OrderType::DEFEND) {
            executeDefendOrder(map, allCharacters);
            moveAlongPath(allCharacters);
            return;  // Don't move aggressively when defending
        }
        
        // No defend order but need resources - stay still but KEEP SHOOTING
        // Don't move to avoid danger, but defend position
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Holding position, needs support\n");
        // Continue to check for orders and shoot, but don't move aggressively
    }
    
    // Healthy and supplied - normal combat behavior
    // Check for visible enemies
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    
    // Execute current order based on type
    if (currentOrder.type == OrderType::ATTACK) {
        executeAttackOrder(map, allCharacters);
        
        // Try to shoot if enemy is visible
        if (visibleEnemy) {
            tryShootEnemy(visibleEnemy, map);
        }
        
        // Clear order if: 
        // 1. No visible enemy AND (path empty OR we've been attacking for a while without seeing enemy)
        // 2. The original target is dead/gone
        if (!visibleEnemy) {
            // If no path or path is nearly complete, clear the order
            if (currentPath.empty() || currentPath.size() <= 2) {
                currentOrder = Order();
                currentPath.clear();  // Clear any remaining path
                pathIndex = 0;
            }
        }
    } else if (currentOrder.type == OrderType::DEFEND) {
        executeDefendOrder(map, allCharacters);
        
        // Shoot at enemies even while defending
        if (visibleEnemy) {
            tryShootEnemy(visibleEnemy, map);
        }
        
        // Clear defend order if path complete
        if (currentPath.empty()) {
            currentOrder = Order();
        }
    } else if (currentOrder.type == OrderType::MOVE) {
        // If we see an enemy while moving, engage them immediately!
        if (visibleEnemy && !enemySightings.empty()) {
            // Clear move order and let commander issue attack order next turn
            currentOrder = Order();
            tryShootEnemy(visibleEnemy, map);
        } else if (currentPath.empty()) {
            // Move order complete
            currentOrder = Order();
        }
    } else if (currentOrder.type == OrderType::NONE) {
        // No orders - engage enemies autonomously if visible
        if (visibleEnemy) {
            tryShootEnemy(visibleEnemy, map);
        }
    }
    
    // Move along current path
    moveAlongPath(allCharacters);
}

/**
 * @brief Execute order from commander
 */
void Warrior::executeOrder(Order order, const Map& map) {
    currentOrder = order;
    
    // Create mutable copy of allCharacters for grenade throwing
    // This is a limitation - we'll handle it in executeAttackOrder
    
    switch (order.type) {
        case OrderType::MOVE:
            executeMoveOrder(map);
            break;
        case OrderType::ATTACK:
            // Will be handled in update with allCharacters
            lastKnownEnemyPosition = order.targetPosition;
            break;
        case OrderType::DEFEND:
            // Will be handled with proper context
            break;
        default:
            break;
    }
}

/**
 * @brief Check ammo and health levels
 */
void Warrior::checkResources() {
    needsAmmo = (ammo <= LOW_AMMO_THRESHOLD);
    needsHealing = (health <= LOW_HEALTH_THRESHOLD);
}

/**
 * @brief Attempt to shoot at enemy
 */
bool Warrior::tryShootEnemy(Character* enemy, const Map& map) {
    if (!enemy || ammo <= 0) return false;
    
    Position enemyPos = enemy->getPosition();
    float distance = position.euclideanDistance(enemyPos);
    
    // Check if in range and has line of sight
    if (distance <= SHOOT_RANGE && AI::hasLineOfSight(position, enemyPos, map)) {
        ammo--;
        enemy->takeDamage(WARRIOR_DAMAGE);
        return true;
    }
    
    return false;
}

/**
 * @brief Attempt to throw grenade
 */
bool Warrior::tryThrowGrenade(const Position& target, const Map& map, std::vector<Character*>& allCharacters) {
    if (grenades <= 0) return false;
    
    float distance = position.euclideanDistance(target);
    
    // Check if in range
    if (distance <= GRENADE_RANGE) {
        grenades--;
        
        // Apply area damage
        for (Character* c : allCharacters) {
            if (!c->isAlive()) continue;
            
            float distToTarget = c->getPosition().euclideanDistance(target);
            if (distToTarget <= GRENADE_RADIUS) {
                // Damage decreases with distance from epicenter
                int damage = static_cast<int>(GRENADE_DAMAGE * (1.0f - distToTarget / GRENADE_RADIUS));
                c->takeDamage(damage);
            }
        }
        return true;
    }
    
    return false;
}

/**
 * @brief Execute attack order
 */
void Warrior::executeAttackOrder(const Map& map, const std::vector<Character*>& allCharacters) {
    if (currentOrder.type != OrderType::ATTACK) return;
    
    Position target = currentOrder.targetPosition;
    
    // Find path to attack position
    Position attackPos = AI::findAttackPosition(position, target, SHOOT_RANGE, map);
    
    if (attackPos != position && currentPath.empty()) {
        // Generate safety map
        std::vector<Position> enemyPos;
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyPos.push_back(c->getPosition());
            }
        }
        auto safetyMap = AI::generateSafetyMap(enemyPos, map);
        
        currentPath = AI::findPath(position, attackPos, map, &safetyMap, 0.3f);
        pathIndex = 0;
    }
}

/**
 * @brief Execute defend order
 */
void Warrior::executeDefendOrder(const Map& map, const std::vector<Character*>& allCharacters) {
    if (currentOrder.type != OrderType::DEFEND) return;
    
    // Generate safety map
    std::vector<Position> enemyPos;
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() != team) {
            enemyPos.push_back(c->getPosition());
        }
    }
    auto safetyMap = AI::generateSafetyMap(enemyPos, map);
    
    // Find safe position
    Position safePos = AI::findNearestSafePosition(position, map, safetyMap, 2.0f);
    
    if (safePos != position && currentPath.empty()) {
        currentPath = AI::findPath(position, safePos, map, &safetyMap, 1.0f);
        pathIndex = 0;
    }
    
    // Still try to shoot if enemy visible
    Character* enemy = findNearestEnemy(allCharacters);
    if (enemy) {
        tryShootEnemy(enemy, map);
    }
}

/**
 * @brief Execute move order
 */
void Warrior::executeMoveOrder(const Map& map) {
    if (currentPath.empty()) {
        currentPath = AI::findPath(position, currentOrder.targetPosition, map);
        pathIndex = 0;
    }
}

/**
 * @brief Evaluate if warrior should retreat
 * Warriors retreat at 40% HP (RETREAT_HEALTH_THRESHOLD) to give time to escape
 * Exit retreat mode when healed above 50% (LOW_HEALTH_THRESHOLD)
 */
void Warrior::evaluateRetreat(const std::vector<Character*>& allCharacters) {
    // Enter retreat mode if health drops to 40% or lower
    if (!isRetreating && health <= RETREAT_HEALTH_THRESHOLD) {
        isRetreating = true;
        
        // Find friendly medic position to retreat towards
        Character* friendlyMedic = nullptr;
        float closestDist = 999999.0f;
        
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::MEDIC) {
                float dist = position.euclideanDistance(c->getPosition());
                if (dist < closestDist) {
                    closestDist = dist;
                    friendlyMedic = c;
                }
            }
        }
        
        if (friendlyMedic) {
            retreatTarget = friendlyMedic->getPosition();
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ENTERING RETREAT MODE! HP:" 
                     << health << "/" << INITIAL_HEALTH << " - Moving to medic at (" 
                     << retreatTarget.x << "," << retreatTarget.y << ")\n");
        } else {
            // No medic available - retreat towards team commander/spawn area
            if (team == Team::BLUE) {
                retreatTarget = Position(5, GRID_HEIGHT / 2);  // Blue spawn area
            } else {
                retreatTarget = Position(GRID_WIDTH - 5, GRID_HEIGHT / 2);  // Orange spawn area
            }
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ENTERING RETREAT MODE! HP:" 
                     << health << "/" << INITIAL_HEALTH << " - No medic, retreating to spawn area\n");
        }
        
        // Clear current orders - retreat takes priority
        currentOrder = Order();
        currentPath.clear();
        pathIndex = 0;
    }
    
    // Exit retreat mode when healed above LOW_HEALTH_THRESHOLD (50%)
    if (isRetreating && health > LOW_HEALTH_THRESHOLD) {
        isRetreating = false;
        needsHealing = false;  // Reset healing flag
        currentPath.clear();
        pathIndex = 0;
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] EXITING RETREAT MODE - Healed to HP:" 
                 << health << "/" << INITIAL_HEALTH << " - Ready for combat!\n");
    }
}

/**
 * @brief Execute retreat behavior
 * Move towards retreat target while maintaining defensive posture
 * Shoot at close enemies but prioritize escape
 */
void Warrior::executeRetreat(const Map& map, const std::vector<Character*>& allCharacters) {
    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] RETREATING to (" 
             << retreatTarget.x << "," << retreatTarget.y << ") | HP:" << health << "/" << INITIAL_HEALTH << "\n");
    
    // Update retreat target if medic has moved (recalculate every few turns)
    static int lastUpdateTurn = -10;
    int currentTurn = lastLoggedTurn;  // Use last logged turn as approximation
    
    if (currentTurn - lastUpdateTurn > 3) {  // Update every 3 turns
        Character* friendlyMedic = nullptr;
        float closestDist = 999999.0f;
        
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::MEDIC) {
                float dist = position.euclideanDistance(c->getPosition());
                if (dist < closestDist) {
                    closestDist = dist;
                    friendlyMedic = c;
                }
            }
        }
        
        if (friendlyMedic) {
            retreatTarget = friendlyMedic->getPosition();
        }
        lastUpdateTurn = currentTurn;
    }
    
    // Defensive shooting - only at close enemies (within 4 tiles)
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    if (visibleEnemy && position.euclideanDistance(visibleEnemy->getPosition()) <= 4) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Defensive shot while retreating\n");
        tryShootEnemy(visibleEnemy, map);
    }
    
    // Move towards retreat target
    // Use pathfinding that avoids enemies (higher safety weight)
    if (currentPath.empty() || currentPath.size() <= 1) {
        // Generate safety map to avoid enemies during retreat
        std::vector<Position> enemyPositions;
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyPositions.push_back(c->getPosition());
            }
        }
        auto safetyMap = AI::generateSafetyMap(enemyPositions, map);
        
        // Find path with high safety weight (avoid enemies more aggressively)
        currentPath = AI::findPath(position, retreatTarget, map, &safetyMap, 2.0f);  // 2.0f = high safety priority
        pathIndex = 0;
        
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No retreat path found! Holding position\n");
        }
    }
    
    // Move along retreat path
    // Note: moveAlongPath checks for friendly units blocking, we'll handle that
    moveAlongPath(allCharacters);
}
