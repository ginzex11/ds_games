#include "warrior.h"

/**
 * @brief Construct a new Warrior
 */
Warrior::Warrior(Position pos, Team t)
    : Character(pos, t, CharacterType::WARRIOR),
      ammo(INITIAL_AMMO), grenades(INITIAL_GRENADES),
      needsAmmo(false), needsHealing(false), isRetreating(false), 
      retreatTarget(pos), previousPosition(pos), failedDestination(-1, -1), 
      failedAttempts(0), turnsSinceLastMove(0), lastLoggedTurn(-1) {
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
    
    // PRIORITY 3: If critically low on resources but not retreating, move towards support
    if (needsHealing || needsAmmo) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Low on resources, seeking support\n");
        
        // Still shoot at ANY visible enemy while moving
        Character* visibleEnemy = findNearestEnemy(allCharacters);
        if (visibleEnemy && ammo > 0) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Defensive shot while seeking support\n");
            tryShootEnemy(visibleEnemy, map);
        }
        
        // ACTIVELY MOVE towards medic/supplier instead of sitting still
        Character* supportUnit = nullptr;
        CharacterType neededType = needsHealing ? CharacterType::MEDIC : CharacterType::SUPPLIER;
        
        // Find friendly medic or supplier
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() == team && c->getType() == neededType) {
                supportUnit = c;
                break;
            }
        }
        
        // Move towards support unit if found and not adjacent
        if (supportUnit) {
            float distance = position.manhattanDistance(supportUnit->getPosition());
            if (distance > 1) {
                // Check if support is already coming towards us (distance decreasing)
                // If support is within reasonable range (10 tiles), WAIT for them
                if (distance <= 10) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Support within range (" 
                             << distance << " tiles), waiting at position\n");
                    currentPath.clear();
                    pathIndex = 0;
                } else {
                    // Support far away - move closer, but path to NEAR support (not exact position)
                    // This prevents multiple warriors from bunching up at the same destination
                    Position supportPos = supportUnit->getPosition();
                    
                    // Find a position NEAR the support unit instead of exact position
                    Position targetPos = supportPos;
                    if (distance > 3) {
                        // If far away, just get closer (path towards them)
                        targetPos = supportPos;
                    } else {
                        // If close, find adjacent free cell to avoid bunching
                        std::vector<Position> adjacentCells = {
                            Position(supportPos.x + 1, supportPos.y),
                            Position(supportPos.x - 1, supportPos.y),
                            Position(supportPos.x, supportPos.y + 1),
                            Position(supportPos.x, supportPos.y - 1)
                        };
                        
                        // Find first free adjacent cell
                        bool foundFree = false;
                        for (const Position& adj : adjacentCells) {
                            if (!isValidPosition(adj) || !map.isPassable(adj)) continue;
                            
                            bool occupied = false;
                            for (Character* c : allCharacters) {
                                if (c != this && c->isAlive() && c->getPosition() == adj) {
                                    occupied = true;
                                    break;
                                }
                            }
                            
                            if (!occupied) {
                                targetPos = adj;
                                foundFree = true;
                                break;
                            }
                        }
                        
                        // If all adjacent cells occupied, just path to support and wait
                        if (!foundFree) {
                            targetPos = supportPos;
                        }
                    }
                    
                    bool needsNewPath = currentPath.empty() || 
                                       pathIndex >= static_cast<int>(currentPath.size()) ||
                                       currentPath.back() != targetPos;
                    
                    if (needsNewPath) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Moving towards " 
                                 << (needsHealing ? "medic" : "supplier") 
                                 << " at (" << supportPos.x << "," << supportPos.y << ")\n");
                        
                        // Use safety map but low weight - getting support is priority
                        std::vector<Position> enemyPos;
                        for (Character* c : allCharacters) {
                            if (c->isAlive() && c->getTeam() != team) {
                                enemyPos.push_back(c->getPosition());
                            }
                        }
                        auto safetyMap = AI::generateSafetyMap(enemyPos, map);
                        currentPath = AI::findPath(position, targetPos, map, &safetyMap, 0.05f);
                        
                        if (currentPath.empty()) {
                            // Fallback - direct path
                            currentPath = AI::findPath(position, targetPos, map);
                        }
                        
                        pathIndex = 0;
                    }
                }
            } else {
                // Adjacent to support - wait for them
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Adjacent to support, waiting\n");
                currentPath.clear();
                pathIndex = 0;
            }
        } else {
            // No support found - hold position
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No support found, holding position\n");
        }
        
        moveAlongPath(allCharacters);
        return;  // Don't execute other orders while seeking support
    }
    
    // Healthy and supplied - normal combat behavior
    // Check for visible enemies
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    
    // UNIVERSAL GRENADE LOGIC: Check grenades BEFORE order execution
    // Grenades are a weapon that works even when out of bullets!
    if (visibleEnemy && grenades > 0) {
        Position enemyPos = visibleEnemy->getPosition();
        float distance = position.euclideanDistance(enemyPos);
        
        // Only consider grenades if enemy is in grenade range
        if (distance <= GRENADE_RANGE) {
            // Count how many enemies would be hit by grenade at this position
            int enemiesInBlastRadius = 0;
            for (Character* c : allCharacters) {
                if (!c->isAlive() || c->getTeam() == team) continue;
                float distToBlast = c->getPosition().euclideanDistance(enemyPos);
                if (distToBlast <= GRENADE_RADIUS) {
                    enemiesInBlastRadius++;
                }
            }
            
            // Use grenade if: 2+ enemies in blast OR only 1 enemy but out of bullets
            if (enemiesInBlastRadius >= 2 || (enemiesInBlastRadius >= 1 && ammo == 0)) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Throwing GRENADE at (" 
                         << enemyPos.x << "," << enemyPos.y 
                         << ") - " << enemiesInBlastRadius << " enemies in blast!"
                         << (ammo == 0 ? " [OUT OF BULLETS]" : "") << "\n");
                
                // Need mutable copy for grenade
                std::vector<Character*> mutableChars = allCharacters;
                if (tryThrowGrenade(enemyPos, map, mutableChars)) {
                    return;  // Grenade thrown, skip other actions this turn
                }
            }
        }
    }
    
    // CRITICAL: If completely out of ammo (0), cannot attack - switch to defensive posture
    if (ammo == 0 && currentOrder.type == OrderType::ATTACK) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] OUT OF AMMO! Switching to DEFEND mode\n");
        currentOrder = Order(OrderType::DEFEND, position);
        currentPath.clear();
        pathIndex = 0;
        needsAmmo = true;  // Ensure commander knows we need resupply
    }
    
    // Execute current order based on type
    if (currentOrder.type == OrderType::ATTACK) {
        executeAttackOrder(map, allCharacters);
        
        // Try to shoot if enemy is visible (normal single-target attack)
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
        // OSCILLATION DETECTION: If we haven't moved for 3+ turns, we're stuck
        if (position == previousPosition) {
            turnsSinceLastMove++;
            
            // Only detect oscillation after being stuck for 3 consecutive turns
            if (turnsSinceLastMove >= 3) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] OSCILLATION DETECTED! Stuck for " 
                         << turnsSinceLastMove << " turns. Marking destination as unreachable\n");
                
                // Mark the destination as failed so we don't try again
                if (!currentPath.empty()) {
                    failedDestination = currentPath.back();
                    failedAttempts = 10;  // Ignore this destination for 10 turns
                }
                
                currentOrder = Order();
                currentPath.clear();
                pathIndex = 0;
                turnsSinceLastMove = 0;
                return;  // Stop trying to move
            }
        } else {
            // We moved! Reset counter
            turnsSinceLastMove = 0;
        }
        
        // If path is empty, calculate it with safety map
        if (currentPath.empty()) {
            executeMoveOrder(map, allCharacters);
            
            // If path still empty after calculation, the destination is unreachable
            if (currentPath.empty()) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Cannot find path to MOVE destination. Clearing order.\n");
                
                // Mark this destination as failed
                failedDestination = currentOrder.targetPosition;
                failedAttempts = 10;  // Don't try this destination for 10 turns
                
                // Clear the MOVE order - don't wander aimlessly!
                currentOrder = Order();
                turnsSinceLastMove = 0;
            }
        } else if (visibleEnemy && !enemySightings.empty()) {
            // If we see an enemy while moving, engage them immediately!
            currentOrder = Order();
            tryShootEnemy(visibleEnemy, map);
        }
    } else if (currentOrder.type == OrderType::NONE) {
        // No orders - engage enemies autonomously if visible
        if (visibleEnemy) {
            float distance = position.euclideanDistance(visibleEnemy->getPosition());
            
            if (distance <= SHOOT_RANGE) {
                // In range - shoot
                tryShootEnemy(visibleEnemy, map);
            } else {
                // Out of range - move closer
                Position enemyPos = visibleEnemy->getPosition();
                if (currentPath.empty() || currentPath.back() != enemyPos) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Enemy spotted out of range, moving to engage\n");
                    currentPath = AI::findPath(position, enemyPos, map);
                    pathIndex = 0;
                }
            }
        }
    }
    
    // Track position before moving (for oscillation detection)
    previousPosition = position;
    
    // Move along current path
    moveAlongPath(allCharacters);
}

/**
 * @brief Execute order from commander
 */
void Warrior::executeOrder(Order order, const Map& map) {
    // Decrement failed attempts counter
    if (failedAttempts > 0) {
        failedAttempts--;
        if (failedAttempts == 0) {
            failedDestination = Position(-1, -1);  // Clear failed destination
        }
    }
    
    // Reject MOVE orders to destinations we recently failed to reach
    if (order.type == OrderType::MOVE && failedAttempts > 0) {
        if (order.targetPosition == failedDestination) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Rejecting MOVE order to unreachable destination (" 
                     << order.targetPosition.x << "," << order.targetPosition.y << ")\n");
            return;  // Don't accept this order
        }
    }
    
    currentOrder = order;
    
    // Create mutable copy of allCharacters for grenade throwing
    // This is a limitation - we'll handle it in executeAttackOrder
    
    switch (order.type) {
        case OrderType::MOVE:
            // Path will be calculated in update() with safety map
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
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] GRENADE EXPLODES at (" 
                 << target.x << "," << target.y << ")!\n");
        
        // Apply area damage
        int hitCount = 0;
        for (Character* c : allCharacters) {
            if (!c->isAlive()) continue;
            
            float distToTarget = c->getPosition().euclideanDistance(target);
            if (distToTarget <= GRENADE_RADIUS) {
                // Damage decreases with distance from epicenter
                int damage = static_cast<int>(GRENADE_DAMAGE * (1.0f - distToTarget / GRENADE_RADIUS));
                c->takeDamage(damage);
                hitCount++;
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Grenade damages " 
                         << characterTypeToString(c->getType()) << " " 
                         << teamToString(c->getTeam()) 
                         << " for " << damage << " damage (dist: " << distToTarget << ")\n");
            }
        }
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Grenade hit " << hitCount << " targets\n");
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
 * @brief Execute move order using safety map
 */
void Warrior::executeMoveOrder(const Map& map, const std::vector<Character*>& allCharacters) {
    if (currentPath.empty()) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Calculating path for MOVE order to (" 
                 << currentOrder.targetPosition.x << "," << currentOrder.targetPosition.y << ") using safety map\n");
        
        // Generate safety map to minimize risk
        std::vector<Position> enemyPos;
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyPos.push_back(c->getPosition());
            }
        }
        auto safetyMap = AI::generateSafetyMap(enemyPos, map);
        
        // Use lower safety weight (0.1f) for MOVE orders to allow long-distance travel
        // while still preferring safer routes when available
        currentPath = AI::findPath(position, currentOrder.targetPosition, map, &safetyMap, 0.1f);
        
        // Fallback: If no path found with safety map, try without it
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No safe path found, trying direct path\n");
            currentPath = AI::findPath(position, currentOrder.targetPosition, map);
        }
        
        // If still no path, try positions near the destination
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Direct path failed, trying nearby positions\n");
            
            // Try positions around the target
            std::vector<Position> nearbyTargets = {
                currentOrder.targetPosition,
                Position(currentOrder.targetPosition.x + 1, currentOrder.targetPosition.y),
                Position(currentOrder.targetPosition.x - 1, currentOrder.targetPosition.y),
                Position(currentOrder.targetPosition.x, currentOrder.targetPosition.y + 1),
                Position(currentOrder.targetPosition.x, currentOrder.targetPosition.y - 1),
                Position(currentOrder.targetPosition.x + 1, currentOrder.targetPosition.y + 1),
                Position(currentOrder.targetPosition.x - 1, currentOrder.targetPosition.y - 1)
            };
            
            for (const Position& nearbyTarget : nearbyTargets) {
                if (isValidPosition(nearbyTarget) && map.isPassable(nearbyTarget)) {
                    currentPath = AI::findPath(position, nearbyTarget, map);
                    if (!currentPath.empty()) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Found path to nearby position (" 
                                 << nearbyTarget.x << "," << nearbyTarget.y << ")\n");
                        break;
                    }
                }
            }
        }
        
        pathIndex = 0;
        
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] WARNING: No path found for MOVE order!\n");
        } else {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Path found with " << currentPath.size() << " steps\n");
        }
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
            // Don't retreat to exact medic position - spread out around medic
            // Use warrior's Y position to determine offset
            int yOffset = (position.y < GRID_HEIGHT / 2) ? -2 : 2;
            retreatTarget = Position(friendlyMedic->getPosition().x, 
                                    friendlyMedic->getPosition().y + yOffset);
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ENTERING RETREAT MODE! HP:" 
                     << health << "/" << INITIAL_HEALTH << " - Moving to medic area at (" 
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

/**
 * @brief Resupply warrior with ammo and grenades (prevents overflow)
 */
void Warrior::resupplyAmmo(int ammoAmount, int grenadeAmount) {
    ammo += ammoAmount;
    if (ammo > INITIAL_AMMO) ammo = INITIAL_AMMO;  // Cap at max
    
    grenades += grenadeAmount;
    if (grenades > INITIAL_GRENADES) grenades = INITIAL_GRENADES;  // Cap at max
    
    needsAmmo = false;
    
    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Resupplied: +" << ammoAmount 
             << " ammo, +" << grenadeAmount << " grenades (now " << ammo << "/" 
             << INITIAL_AMMO << " ammo, " << grenades << "/" << INITIAL_GRENADES << " grenades)\n");
}

/**
 * @brief Heal warrior (prevents overflow)
 */
void Warrior::heal(int amount) {
    int oldHealth = health;
    health += amount;
    if (health > INITIAL_HEALTH) health = INITIAL_HEALTH;  // Cap at max
    
    needsHealing = false;
    
    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Healed: +" << (health - oldHealth) 
             << " HP (now " << health << "/" << INITIAL_HEALTH << ")\n");
}
