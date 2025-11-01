#include "warrior.h"
#include "medic.h"
#include "supplier.h"
#include <cmath>

/**
 * @brief Construct a new Warrior
 */
Warrior::Warrior(Position pos, Team t)
    : Character(pos, t, CharacterType::WARRIOR),
      ammo(INITIAL_AMMO), grenades(INITIAL_GRENADES),
      needsAmmo(false), needsHealing(false), isRetreating(false), retreatSprintActive(false),
      retreatTarget(pos), previousPosition(pos), failedDestination(-1, -1), 
      failedAttempts(0), autonomousCooldown(0), turnsSinceLastMove(0), 
      turnsWaitingForMedic(0), lastKnownMedicDistance(999.0f), turnsWithoutMedicProgress(0), lastLoggedTurn(-1),
      lastStandCooldown(0), turnsWithZeroAmmo(0),
      turnsWithoutEnemySighting(0), lastEnemySeenPosition(-1, -1),
      turnsSinceLastDamage(999), lastDamageTurn(-1),
      lastShotTarget(-1, -1), lastGrenadeTarget(-1, -1), shotThisTurn(false), grenadeThisTurn(false) {
}

/**
 * @brief Update warrior - execute orders or act autonomously
 */
void Warrior::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Reset visual effect flags at start of turn
    shotThisTurn = false;
    grenadeThisTurn = false;
    
    // Update visibility and scan
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
    // Check resource status
    checkResources();
    
    // FIX 2: Track turns with 0 ammo for emergency protocol
    if (ammo == 0) {
        turnsWithZeroAmmo++;
        
        // Emergency protocol: If stuck with 0 ammo for 5+ turns and no grenades, enable melee mode
        if (turnsWithZeroAmmo >= 5 && grenades == 0) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] EMERGENCY: 0 ammo for " 
                     << turnsWithZeroAmmo << " turns! Entering MELEE MODE - will charge enemies!\n");
            autonomousCooldown = 15;  // Act independently for 15 turns
            currentOrder = Order();   // Clear current orders
        }
    } else {
        turnsWithZeroAmmo = 0;  // Reset counter when we get ammo
    }
    
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
    // BUT: Allow defensive shooting while retreating
    if (isRetreating) {
        // Check for enemies in range and shoot defensively while retreating
        Character* visibleEnemy = findNearestEnemy(allCharacters);
        if (visibleEnemy) {
            float distance = position.euclideanDistance(visibleEnemy->getPosition());
            if (distance <= SHOOT_RANGE && ammo > 0) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] DEFENSIVE FIRE while retreating\n");
                tryShootEnemy(visibleEnemy, map);
                // Don't return - continue with retreat movement
            }
        }
        
        executeRetreat(map, allCharacters);
        return;  // Retreat is top priority - exit early
    }
    
    // PRIORITY 2.5: If in autonomous mode cooldown, ignore commander orders
    // This allows warrior to make independent tactical decisions after path failures
    if (autonomousCooldown > 0) {
        autonomousCooldown--;
        
        // FIX 3: Better logging - distinguish between normal autonomous and Last Stand
        if (autonomousCooldown >= 50) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] LAST STAND MODE (" 
                     << autonomousCooldown << " turns remaining) - fighting independently\n");
        } else {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Acting autonomously (" 
                     << autonomousCooldown << " turns remaining) - ignoring commander orders\n");
        }
        
        // Clear any order that might have been assigned
        currentOrder = Order();
        
        // Act autonomously (handled in PRIORITY 4 below)
        // Continue to resource check and autonomous behavior sections
    }
    
    // PRIORITY 3: If CRITICALLY low on resources (no ammo OR very low health), move towards support
    // Only interrupt orders if resources are CRITICAL, not just "low"
    bool criticallyLowResources = (ammo == 0 && needsAmmo) || (health <= 20 && needsHealing);
    
    if (criticallyLowResources) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Low on resources, seeking support\n");
        
        // SPECIAL CASE: If out of ammo, try grenades first!
        Character* visibleEnemy = findNearestEnemy(allCharacters);
        if (ammo == 0 && grenades > 0 && visibleEnemy) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] OUT OF AMMO! Using grenades as fallback weapon\n");
            Position enemyPos = visibleEnemy->getPosition();
            std::vector<Character*> mutableChars = allCharacters;
            if (tryThrowGrenade(enemyPos, map, mutableChars)) {
                return;  // Grenade thrown, done for this turn
            }
        }
        
        // Still shoot at ANY visible enemy while moving (if we have ammo)
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
            // CRITICAL: Check if support actually HAS supplies to give
            bool hasSupplies = false;
            if (needsHealing) {
                Medic* medic = static_cast<Medic*>(supportUnit);
                hasSupplies = medic->hasMedicine();
            } else {
                Supplier* supplier = static_cast<Supplier*>(supportUnit);
                hasSupplies = supplier->hasAmmo();
            }
            
            if (!hasSupplies) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Support has no supplies! Going to warehouse instead\n");
                supportUnit = nullptr;  // Trigger warehouse fallback below
            }
        }
        
        // Move towards support unit if found and not adjacent
        if (supportUnit) {
            float distance = position.manhattanDistance(supportUnit->getPosition());
            if (distance > 1) {
                // FIX: Only wait for support if STILL critically low on resources
                // If we got resupplied already, don't keep waiting!
                bool stillCritical = (ammo == 0 && needsAmmo) || (health <= 20 && needsHealing);
                
                // Check if support is already coming towards us (distance decreasing)
                // If support is within reasonable range (10 tiles) AND we're still critical, WAIT
                if (distance <= 10 && stillCritical) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Support within range (" 
                             << distance << " tiles), HOLDING POSITION for rendezvous\n");
                    currentPath.clear();
                    pathIndex = 0;
                    return;  // CRITICAL: Actually stop moving! Don't continue to warehouse logic
                } else if (!stillCritical) {
                    // We got resupplied! Exit support-seeking mode
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ✅ Got supplies! Exiting support-seeking mode\n");
                    // Fall through to normal combat logic below
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
            // No support found - last resort: go to warehouse!
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No support found, heading to warehouse!\n");
            
            // Find team's warehouse
            Position warehousePos;
            if (needsHealing) {
                warehousePos = map.getWarehouse(team, WarehouseType::MEDICINE);
            } else {
                warehousePos = map.getWarehouse(team, WarehouseType::AMMO);
            }
            
            // Path to warehouse if not already there
            if (position.manhattanDistance(warehousePos) > 1) {
                if (currentPath.empty() || currentPath.back() != warehousePos) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Moving to warehouse at (" 
                             << warehousePos.x << "," << warehousePos.y << ")\n");
                    currentPath = AI::findPath(position, warehousePos, map);
                    pathIndex = 0;
                }
            } else {
                // At warehouse - hold position (maybe support will spawn or commander will help)
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] At warehouse, holding position\n");
                currentPath.clear();
                pathIndex = 0;
            }
        }
        
        moveAlongPath(allCharacters);
        return;  // Don't execute other orders while seeking support
    }
    
    // Healthy and supplied - normal combat behavior
    // Check for visible enemies
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    
    // PRIORITY 2: Track enemy sightings for active seeking behavior
    if (visibleEnemy) {
        turnsWithoutEnemySighting = 0;  // Reset counter when enemy seen
        lastEnemySeenPosition = visibleEnemy->getPosition();  // Remember where
    } else {
        turnsWithoutEnemySighting++;  // Increment counter when no enemies visible
    }
    
    // SOLUTION 2: Add visibility diagnostics
    if (visibleEnemy) {
        Position enemyPos = visibleEnemy->getPosition();
        float dist = position.euclideanDistance(enemyPos);
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Enemy detected at (" 
                 << enemyPos.x << "," << enemyPos.y << ") | Distance: " << dist 
                 << " | Visible cells: " << visibleCells.size() << "\n");
    } else {
        // SOLUTION 3: Debug why no enemies found - ENHANCED DIAGNOSTICS
        int enemyCount = 0;
        int visibleEnemyCount = 0;
        
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyCount++;
                if (canSee(c->getPosition())) {
                    visibleEnemyCount++;
                }
            }
        }
        
        if (enemyCount > 0 && visibleEnemyCount == 0) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] DEBUG: " << enemyCount 
                     << " enemies alive but 0 visible (visibility: " << visibleCells.size() 
                     << " cells)\n");
        }
    }
    
    // UNIVERSAL GRENADE LOGIC: Check grenades BEFORE order execution
    // Grenades are a weapon that works even when out of bullets!
    if (visibleEnemy && grenades > 0) {
        Position enemyPos = visibleEnemy->getPosition();
        float distance = position.euclideanDistance(enemyPos);
        
        // Only consider grenades if enemy is in grenade range
        if (distance <= GRENADE_RANGE) {
            // SAFETY CHECK: Count enemies AND friendlies in blast radius
            int enemiesInBlastRadius = 0;
            int friendliesInBlastRadius = 0;
            int criticalFriendliesInBlastRadius = 0;  // Medics or Suppliers
            
            for (Character* c : allCharacters) {
                if (!c->isAlive()) continue;
                float distToBlast = c->getPosition().euclideanDistance(enemyPos);
                if (distToBlast <= GRENADE_RADIUS) {
                    if (c->getTeam() == team) {
                        friendliesInBlastRadius++;
                        // CRITICAL: Check for support units
                        if (c->getType() == CharacterType::MEDIC || 
                            c->getType() == CharacterType::SUPPLIER) {
                            criticalFriendliesInBlastRadius++;
                        }
                    } else {
                        enemiesInBlastRadius++;
                    }
                }
            }
            
            // SAFETY CHECK 1: NEVER throw if it would hit support units
            if (criticalFriendliesInBlastRadius > 0) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                         << "] Grenade would hit " << criticalFriendliesInBlastRadius 
                         << " support units (Medic/Supplier)! ABORTING throw.\n");
            }
            // SAFETY CHECK 2: Only throw if benefit > cost (enemies >= friendlies * 2)
            else if (friendliesInBlastRadius > 0 && enemiesInBlastRadius < friendliesInBlastRadius * 2) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                         << "] Grenade risk/reward poor (" << enemiesInBlastRadius 
                         << " enemies vs " << friendliesInBlastRadius 
                         << " friendlies). Not throwing.\n");
            }
            // Safe to throw: no friendlies OR benefit outweighs cost
            else if (enemiesInBlastRadius >= 2 || (enemiesInBlastRadius >= 1 && ammo == 0)) {
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
    // FIX 2: Unless in emergency melee mode (5+ turns without ammo), then charge enemies!
    if (ammo == 0 && currentOrder.type == OrderType::ATTACK) {
        if (turnsWithZeroAmmo >= 5 && grenades == 0) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] OUT OF AMMO! But in MELEE MODE - charging enemy!\n");
            // Keep ATTACK order active - will path to enemy and try melee when adjacent
        } else {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] OUT OF AMMO! Switching to DEFEND mode\n");
            currentOrder = Order(OrderType::DEFEND, position);
            currentPath.clear();
            pathIndex = 0;
            needsAmmo = true;  // Ensure commander knows we need resupply
        }
    }
    
    // Execute current order based on type
    if (currentOrder.type == OrderType::ATTACK) {
        // SOLUTION 3: Log combat attempt details
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Executing ATTACK order | " 
                 << "Ammo: " << ammo << "/" << INITIAL_AMMO 
                 << " | Grenades: " << grenades << "/" << INITIAL_GRENADES 
                 << " | Enemy visible: " << (visibleEnemy ? "YES" : "NO") << "\n");
        
        // STUCK DETECTION: If we haven't moved while attacking, we may be stuck
        if (position == previousPosition) {
            turnsSinceLastMove++;
            
            // If stuck for 5+ turns during ATTACK, clear the order
            // BUT: Only mark as "failed" if we truly can't path (not just LOS blocked)
            if (turnsSinceLastMove >= 5) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] STUCK while attacking for " 
                         << turnsSinceLastMove << " turns. Clearing ATTACK order\n");
                
                // DON'T mark as failed - might just be LOS blocked, not pathfinding issue
                // Let commander try again with potentially different positioning
                
                // ENTER AUTONOMOUS MODE: Warrior makes own tactical decisions for 8 turns (reduced from 15)
                autonomousCooldown = 8;
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Switching to AUTONOMOUS mode for 8 turns\n");
                
                currentOrder = Order();
                currentPath.clear();
                pathIndex = 0;
                turnsSinceLastMove = 0;
                return;
            }
        } else {
            turnsSinceLastMove = 0;
        }
        
        executeAttackOrder(map, allCharacters);
        
        // Try to shoot if enemy is visible (normal single-target attack)
        if (visibleEnemy) {
            bool shotFired = tryShootEnemy(visibleEnemy, map);
            // SOLUTION 3: Log if shooting failed despite visible enemy
            if (!shotFired && ammo > 0) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Failed to shoot visible enemy (range/LOS issue)\n");
            }
        }
        
        // Only clear ATTACK order if we've truly lost all targets:
        // 1. No visible enemies at all
        // 2. Haven't seen any enemies recently (sightings empty or old)
        // 3. Path is completely exhausted
        if (!visibleEnemy && enemySightings.empty() && currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No enemies found, clearing ATTACK order\n");
            currentOrder = Order();
            pathIndex = 0;
        }
        
        // If order was cleared/rejected and we have visible enemies, engage autonomously
        if (currentOrder.type == OrderType::NONE && visibleEnemy) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Order cleared but enemy visible - engaging autonomously\n");
            // Don't return - fall through to autonomous behavior below
        }
    } else if (currentOrder.type == OrderType::DEFEND) {
        // SOLUTION 3: Log defend order execution
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Executing DEFEND order | " 
                 << "Enemy visible: " << (visibleEnemy ? "YES" : "NO") 
                 << " | Ammo: " << ammo << "\n");
        
        executeDefendOrder(map, allCharacters);
        
        // Shoot at enemies even while defending
        if (visibleEnemy) {
            bool shotFired = tryShootEnemy(visibleEnemy, map);
            // SOLUTION 3: Log defend shooting result
            if (!shotFired && ammo > 0) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Failed to shoot while defending (range/LOS issue)\n");
            }
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
    }
    
    // AUTONOMOUS BEHAVIOR: No orders - engage enemies autonomously if visible
    // This runs independently and can trigger even after rejecting/clearing orders
    if (currentOrder.type == OrderType::NONE) {
        // No orders - engage enemies autonomously if visible
        if (visibleEnemy) {
            float distance = position.euclideanDistance(visibleEnemy->getPosition());
            Position enemyPos = visibleEnemy->getPosition();
            
            // Check if this enemy position is marked as failed
            if (failedAttempts > 0 && enemyPos == failedDestination) {
                // This target is unreachable, look for a different enemy
                Character* alternateEnemy = nullptr;
                float bestDistance = 999999.0f;
                
                for (Character* c : allCharacters) {
                    if (!c->isAlive() || c->getTeam() == team) continue;
                    
                    Position altPos = c->getPosition();
                    // Skip if this is also a failed destination
                    if (altPos == failedDestination) continue;
                    
                    float dist = position.euclideanDistance(altPos);
                    if (dist < bestDistance) {
                        bestDistance = dist;
                        alternateEnemy = c;
                    }
                }
                
                visibleEnemy = alternateEnemy;  // May be null if all visible enemies are unreachable
                if (!visibleEnemy) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] All visible enemies unreachable, holding position\n");
                    return;  // Can't engage anyone
                }
                
                enemyPos = visibleEnemy->getPosition();
                distance = position.euclideanDistance(enemyPos);
            }
            
            if (distance <= SHOOT_RANGE) {
                // In range - shoot
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] AUTONOMOUS: Enemy in range, attempting to shoot\n");
                bool shotFired = tryShootEnemy(visibleEnemy, map);
                
                // FIX: If shot blocked, try to reposition for better angle
                if (!shotFired && ammo > 0) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] AUTONOMOUS: Shot blocked! Repositioning for better angle\n");
                    
                    // Try to find a position with clear line of sight
                    Position bestPos = position;
                    bool foundClearShot = false;
                    
                    // Check positions around current location (radius 2-4 tiles)
                    for (int radius = 2; radius <= 4 && !foundClearShot; radius++) {
                        for (int dx = -radius; dx <= radius; dx++) {
                            for (int dy = -radius; dy <= radius; dy++) {
                                if (dx == 0 && dy == 0) continue;
                                
                                Position testPos(position.x + dx, position.y + dy);
                                if (!isValidPosition(testPos) || !map.isPassable(testPos)) continue;
                                
                                // Check if this position has LOS to enemy
                                if (AI::hasLineOfSight(testPos, enemyPos, map)) {
                                    float testDist = testPos.euclideanDistance(enemyPos);
                                    if (testDist <= SHOOT_RANGE) {
                                        bestPos = testPos;
                                        foundClearShot = true;
                                        break;
                                    }
                                }
                            }
                            if (foundClearShot) break;
                        }
                    }
                    
                    // If found better position, path to it
                    if (foundClearShot && bestPos != position) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Found clear shot position at (" 
                                 << bestPos.x << "," << bestPos.y << "), moving there\n");
                        currentPath = AI::findPath(position, bestPos, map);
                        if (!currentPath.empty()) {
                            pathIndex = 0;
                            return;
                        }
                    } else {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No clear shot positions found nearby\n");
                    }
                }
            } else {
                // Out of range - find attack position (not exact enemy location)
                Position attackPos = AI::findAttackPosition(position, enemyPos, SHOOT_RANGE, map);
                
                // Only recalculate path if we need a new one
                if (currentPath.empty() || currentPath.back() != attackPos) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Enemy spotted out of range, moving to engage\n");
                    
                    // Try pathfinding with safety consideration
                    std::vector<Position> enemyPositions;
                    for (Character* c : allCharacters) {
                        if (c->isAlive() && c->getTeam() != team) {
                            enemyPositions.push_back(c->getPosition());
                        }
                    }
                    auto safetyMap = AI::generateSafetyMap(enemyPositions, map);
                    currentPath = AI::findPath(position, attackPos, map, &safetyMap, 0.3f);
                    
                    // If no safe path, try direct
                    if (currentPath.empty()) {
                        currentPath = AI::findPath(position, attackPos, map);
                    }
                    
                    // If still no path, mark this target as failed
                    if (currentPath.empty()) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Cannot path to primary target at (" 
                                 << enemyPos.x << "," << enemyPos.y << "), trying alternate targets\n");
                        failedDestination = enemyPos;
                        failedAttempts = 10;
                        
                        // FALLBACK 1: Try finding a DIFFERENT visible enemy
                        Character* alternateEnemy = nullptr;
                        float closestDist = 999999.0f;
                        for (Character* c : allCharacters) {
                            if (c->isAlive() && c->getTeam() != team && c->getPosition() != enemyPos) {
                                float dist = position.euclideanDistance(c->getPosition());
                                if (dist < closestDist) {
                                    closestDist = dist;
                                    alternateEnemy = c;
                                }
                            }
                        }
                        
                        if (alternateEnemy) {
                            Position altPos = alternateEnemy->getPosition();
                            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Found alternate enemy at (" 
                                     << altPos.x << "," << altPos.y << "), distance: " << closestDist << "\n");
                            
                            // Try path to alternate
                            Position altAttackPos(altPos.x, altPos.y);
                            currentPath = AI::findPath(position, altAttackPos, map, &safetyMap, 0.3f);
                            if (currentPath.empty()) {
                                currentPath = AI::findPath(position, altAttackPos, map);
                            }
                            
                            if (!currentPath.empty()) {
                                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Engaging alternate target\n");
                                pathIndex = 0;
                                return;  // Successfully found alternate
                            }
                        }
                        
                        // FALLBACK 2: No reachable enemies - ACTIVE SEEKING if enemies exist but not visible
                        // PRIORITY 2: Don't just hold position - actively seek enemies
                        if (turnsWithoutEnemySighting >= 10) {
                            // Count total enemies alive
                            int aliveEnemies = 0;
                            for (Character* c : allCharacters) {
                                if (c->isAlive() && c->getTeam() != team) {
                                    aliveEnemies++;
                                }
                            }
                            
                            if (aliveEnemies > 0) {
                                Position seekTarget;
                                
                                // PRIORITY 2: Anti-separation check - return to team if too far
                                if (isTooFarFromTeam(allCharacters)) {
                                    seekTarget = calculateTeamCenter(allCharacters);
                                    LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                                             << "] ANTI-SEPARATION: Too far from team! Returning to team center at (" 
                                             << seekTarget.x << "," << seekTarget.y << ")\n");
                                }
                                // Strategy 1: Move toward last known enemy position if we have one
                                else if (lastEnemySeenPosition.x >= 0 && lastEnemySeenPosition.y >= 0) {
                                    seekTarget = lastEnemySeenPosition;
                                    LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                                             << "] ACTIVE SEEKING: No enemies visible for " 
                                             << turnsWithoutEnemySighting << " turns. Moving toward last known position (" 
                                             << seekTarget.x << "," << seekTarget.y << ")\n");
                                } else {
                                    // Strategy 2: Move toward enemy territory center
                                    if (team == Team::BLUE) {
                                        seekTarget = Position(GRID_WIDTH * 3 / 4, GRID_HEIGHT / 2);  // Orange side
                                    } else {
                                        seekTarget = Position(GRID_WIDTH / 4, GRID_HEIGHT / 2);  // Blue side
                                    }
                                    LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                                             << "] ACTIVE SEEKING: Moving toward enemy territory at (" 
                                             << seekTarget.x << "," << seekTarget.y << ")\n");
                                }
                                
                                // Path to seek target
                                currentPath = AI::findPath(position, seekTarget, map);
                                if (!currentPath.empty()) {
                                    pathIndex = 0;
                                    return;  // Start seeking
                                }
                            }
                        }
                        
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No reachable enemies. Holding defensive position, will shoot on sight.\n");
                        currentPath.clear();
                        pathIndex = 0;
                        return;
                    } else {
                        pathIndex = 0;
                    }
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
    
    // SOLUTION 1: Add shooting logs
    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Attempting to shoot enemy at (" 
             << enemyPos.x << "," << enemyPos.y << ") | Dist: " << distance 
             << " | Range: " << SHOOT_RANGE << " | Ammo: " << ammo << "\n");
    
    // Check if in range and has line of sight
    if (distance <= SHOOT_RANGE) {
        bool hasLOS = AI::hasLineOfSight(position, enemyPos, map);
        
        if (hasLOS) {
            ammo--;
            enemy->takeDamage(WARRIOR_DAMAGE);
            
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] *** FIRING at enemy at (" 
                     << enemyPos.x << "," << enemyPos.y << ")! *** Damage: " << WARRIOR_DAMAGE 
                     << " | Ammo remaining: " << ammo << "\n");
            
            // Track for visual effects
            shotThisTurn = true;
            lastShotTarget = enemyPos;
            
            return true;
        } else {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Shot BLOCKED - No line of sight to enemy\n");
        }
    } else {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Target OUT OF RANGE (need " 
                 << distance - SHOOT_RANGE << " tiles closer)\n");
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
        
        // Track for visual effects
        grenadeThisTurn = true;
        lastGrenadeTarget = target;
        
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
    
    // REJECT FAILED TARGETS: Don't accept orders to targets we recently failed to reach
    if (failedAttempts > 0 && currentOrder.targetPosition == failedDestination) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Rejecting ATTACK order to failed target at (" 
                 << failedDestination.x << "," << failedDestination.y 
                 << ") - failed " << (10 - failedAttempts + 1) << " turns ago. Switching to autonomous mode.\n");
        
        // ENTER AUTONOMOUS MODE: Warrior makes own tactical decisions for next 15 turns
        autonomousCooldown = 15;
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Entering AUTONOMOUS mode for 15 turns\n");
        
        currentOrder = Order();  // Clear the order - will trigger autonomous behavior
        currentPath.clear();
        pathIndex = 0;
        
        // DON'T RETURN - fall through to check for autonomous enemy engagement
        // The order is now NONE, so we should look for enemies to engage
    }
    
    // ACTIVE HUNTING: Look for visible enemies first
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    
    // FIX 2: TACTICAL OVERRIDE - Direct distance check instead of relying on visibility
    // The problem: findNearestEnemy uses canSee() which depends on pre-computed visibleCells
    // If visibility system has bugs, we never find enemies to override with
    // Solution: Manually check enemy distances when target not visible
    bool canSeeOrderedTarget = canSee(currentOrder.targetPosition);
    
    if (!canSeeOrderedTarget && !visibleEnemy) {
        // Target not visible AND findNearestEnemy found nothing
        // Do a DIRECT distance check to find any enemy within reasonable range
        Character* closestEnemy = nullptr;
        float minDist = std::numeric_limits<float>::max();
        
        for (Character* c : allCharacters) {
            if (!c->isAlive() || c->getTeam() == team) continue;
            
            Position enemyPos = c->getPosition();
            float dist = position.euclideanDistance(enemyPos);
            
            // If enemy is within extended range (visibility + buffer), consider them
            if (dist <= VISIBILITY_RANGE + 3 && dist < minDist) {
                minDist = dist;
                closestEnemy = c;
            }
        }
        
        if (closestEnemy) {
            Position enemyPos = closestEnemy->getPosition();
            float distToClose = position.euclideanDistance(enemyPos);
            float distToOrdered = position.euclideanDistance(currentOrder.targetPosition);
            
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] TACTICAL OVERRIDE (Direct Check): "
                     << "Ordered target at (" << currentOrder.targetPosition.x << "," 
                     << currentOrder.targetPosition.y << ") not visible, but enemy within range at (" 
                     << enemyPos.x << "," << enemyPos.y << ") | Dist: " << distToClose 
                     << " vs ordered: " << distToOrdered << "\n");
            
            // Override if close enemy is nearer or ordered target very far
            if (distToClose < distToOrdered || distToOrdered > 15.0f) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Engaging nearby enemy instead of distant target\n");
                currentOrder.targetPosition = enemyPos;
                visibleEnemy = closestEnemy;  // Treat as visible for combat logic
            }
        }
    } else if (!canSeeOrderedTarget && visibleEnemy) {
        // Original override logic for when findNearestEnemy works
        Position enemyPos = visibleEnemy->getPosition();
        float distToVisible = position.euclideanDistance(enemyPos);
        float distToOrdered = position.euclideanDistance(currentOrder.targetPosition);
        
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] TACTICAL OVERRIDE: Ordered target at (" 
                 << currentOrder.targetPosition.x << "," << currentOrder.targetPosition.y 
                 << ") not visible, but enemy visible at (" << enemyPos.x << "," << enemyPos.y 
                 << ") | Dist visible: " << distToVisible << " | Dist ordered: " << distToOrdered << "\n");
        
        if (distToVisible < distToOrdered || distToOrdered > 15.0f) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Engaging visible enemy instead of blind pathfinding\n");
            currentOrder.targetPosition = enemyPos;
        }
    }
    
    // If we can see an enemy, move toward them (not the old target position)
    Position target = visibleEnemy ? visibleEnemy->getPosition() : currentOrder.targetPosition;
    
    // Find path to attack position (within shooting range of target)
    Position attackPos = AI::findAttackPosition(position, target, SHOOT_RANGE, map);
    
    // Calculate new path if:
    // 1. No current path (just starting or path completed)
    // 2. Target changed (enemy moved or new enemy spotted)
    // 3. We're not at a good attack position
    bool needNewPath = currentPath.empty() || 
                       (visibleEnemy && currentOrder.targetPosition != target) ||
                       (attackPos != position && position.euclideanDistance(attackPos) > 2) ||
                       (!visibleEnemy && attackPos != position);  // Always create path when no enemy visible
    
    if (needNewPath && attackPos != position) {
        // Update target if we found a visible enemy
        if (visibleEnemy) {
            currentOrder.targetPosition = target;
        }
        
        // Generate safety map
        std::vector<Position> enemyPos;
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyPos.push_back(c->getPosition());
            }
        }
        auto safetyMap = AI::generateSafetyMap(enemyPos, map);
        
        currentPath = AI::findPath(position, attackPos, map, &safetyMap, 0.3f);
        
        // If no path found with safety map, try direct path
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] No safe path to attack position, trying direct path\n");
            currentPath = AI::findPath(position, attackPos, map);
        }
        
        // If still no path, the target may be unreachable - mark this
        if (currentPath.empty()) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] WARNING: Cannot reach attack position for target at (" 
                     << target.x << "," << target.y << ")\n");
            // Don't clear order yet - stuck detection will handle it after 5 turns
        } else {
            pathIndex = 0;
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Created path with " << currentPath.size() 
                     << " steps, advancing toward " 
                     << (visibleEnemy ? "VISIBLE enemy" : "target") 
                     << " at (" << target.x << "," << target.y << ")\n");
        }
    }
}

/**
 * @brief Execute defend order using depth-limited BFS
 * Per assignment: "יש להגדיר טווח החיפוש" (must define search range)
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
    
    // Find safe position using depth-limited BFS with 15-tile search range
    // Assignment requirement: warriors must search for nearest safe point with defined range
    Position safePos = AI::findNearestSafePosition(position, map, safetyMap, 2.0f, 15);
    
    if (safePos != position && currentPath.empty()) {
        currentPath = AI::findPath(position, safePos, map, &safetyMap, 1.0f);
        pathIndex = 0;
    }
    
    // Still try to shoot if enemy visible (defensive posture)
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
 * NEW: Tactical Withdrawal System
 * - At 100 HP (67%): Preemptive retreat if recently damaged (within 3 turns) AND medic exists
 * - At 50 HP (33%): Standard retreat if medic exists
 * - At 30 HP (20%): Emergency retreat if no medic
 * This creates dynamic "dance" behavior - warriors disengage under fire, re-engage when healed
 */
void Warrior::evaluateRetreat(const std::vector<Character*>& allCharacters) {
    // Track health changes to detect damage
    static std::unordered_map<Warrior*, int> lastHealthMap;
    
    // Initialize if first time seeing this warrior
    if (lastHealthMap.find(this) == lastHealthMap.end()) {
        lastHealthMap[this] = health;
    }
    
    int lastHealth = lastHealthMap[this];
    if (lastHealth > health) {
        // Took damage this turn!
        turnsSinceLastDamage = 0;
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] 🔥 UNDER FIRE! Lost " 
                 << (lastHealth - health) << " HP (now " << health << "/" << INITIAL_HEALTH 
                 << ") | Damage tracking: " << turnsSinceLastDamage << " turns ago\n");
    } else {
        turnsSinceLastDamage++;
    }
    lastHealthMap[this] = health;
    
    // FIX 1: Decrement Last Stand cooldown
    if (lastStandCooldown > 0) {
        lastStandCooldown--;
        if (lastStandCooldown > 0) {
            // Still in cooldown - cannot enter retreat mode yet
            return;
        }
    }
    
    // Check if medic exists on team
    bool medicExists = false;
    Character* friendlyMedic = nullptr;
    float closestDist = 999999.0f;
    
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::MEDIC) {
            medicExists = true;
            float dist = position.euclideanDistance(c->getPosition());
            if (dist < closestDist) {
                closestDist = dist;
                friendlyMedic = c;
            }
        }
    }
    
    // TACTICAL WITHDRAWAL: Preemptive retreat at 67% HP if under active fire
    bool underActiveFire = (turnsSinceLastDamage <= 3);  // Damaged in last 3 turns
    bool shouldTacticallyWithdraw = false;
    
    // Debug logging for tactical withdrawal evaluation (only when health is in tactical range)
    if (health <= TACTICAL_WITHDRAWAL_THRESHOLD && health > RETREAT_HEALTH_THRESHOLD) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Tactical withdrawal check: "
                 << "HP=" << health << "/" << INITIAL_HEALTH 
                 << " | Medic=" << (medicExists ? "YES" : "NO")
                 << " | UnderFire=" << (underActiveFire ? "YES" : "NO")
                 << " | TurnsSinceDamage=" << turnsSinceLastDamage << "\n");
    }
    
    if (medicExists && underActiveFire && health <= TACTICAL_WITHDRAWAL_THRESHOLD && health > RETREAT_HEALTH_THRESHOLD) {
        shouldTacticallyWithdraw = true;
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ⚠️ TACTICAL WITHDRAWAL TRIGGERED! "
                 << "HP: " << health << "/" << INITIAL_HEALTH << " (67%) | Under fire (damaged " 
                 << turnsSinceLastDamage << " turns ago) | Disengaging to heal\n");
    }
    
    // DYNAMIC THRESHOLD: Standard retreat thresholds
    int retreatThreshold = medicExists ? RETREAT_HEALTH_THRESHOLD : (INITIAL_HEALTH * 0.2f);
    
    // Enter retreat mode if:
    // 1. Tactical withdrawal triggered (67% HP + under fire + medic exists)
    // 2. Standard retreat threshold reached
    if (!isRetreating && (shouldTacticallyWithdraw || health <= retreatThreshold)) {
        isRetreating = true;
        retreatSprintActive = true;  // SPRINT MECHANIC: Enable fast escape on first turn
        
        if (friendlyMedic) {
            Position medicPos = friendlyMedic->getPosition();
            
            // Retreat TOWARDS medic position, not away from it!
            // Find a safe rally point between warrior and medic
            int targetX = medicPos.x;
            int targetY = medicPos.y;
            
            // If medic is far away, retreat halfway towards medic instead of all the way
            float distToMedic = position.euclideanDistance(medicPos);
            if (distToMedic > 10) {
                // Retreat halfway towards medic
                targetX = (position.x + medicPos.x) / 2;
                targetY = (position.y + medicPos.y) / 2;
            }
            // If medic is close (within 10 tiles), retreat directly to medic area
            
            retreatTarget = Position(targetX, targetY);
            
            if (shouldTacticallyWithdraw) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] 🏃 TACTICAL RETREAT MODE! "
                         << "Disengaging early to medic at (" 
                         << retreatTarget.x << "," << retreatTarget.y << ") to avoid defeat\n");
            } else {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ENTERING RETREAT MODE! HP:" 
                         << health << "/" << INITIAL_HEALTH << " (threshold: " << retreatThreshold 
                         << ") - Retreating to medic area at (" 
                         << retreatTarget.x << "," << retreatTarget.y << ")\n");
            }
        } else {
            // No medic available - retreat towards team commander/spawn area
            if (team == Team::BLUE) {
                retreatTarget = Position(5, GRID_HEIGHT / 2);  // Blue spawn area
            } else {
                retreatTarget = Position(GRID_WIDTH - 5, GRID_HEIGHT / 2);  // Orange spawn area
            }
            
            // CRITICAL: Enforce map bounds safety margin (stay 2 tiles from edges)
            if (retreatTarget.x < 2) retreatTarget.x = 2;
            if (retreatTarget.y < 2) retreatTarget.y = 2;
            if (retreatTarget.x >= GRID_WIDTH - 2) retreatTarget.x = GRID_WIDTH - 3;
            if (retreatTarget.y >= GRID_HEIGHT - 2) retreatTarget.y = GRID_HEIGHT - 3;
            
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ENTERING RETREAT MODE! HP:" 
                     << health << "/" << INITIAL_HEALTH << " - No medic, retreating to spawn area at (" 
                     << retreatTarget.x << "," << retreatTarget.y << ")\n");
        }
        
        // Clear current orders - retreat takes priority
        currentOrder = Order();
        currentPath.clear();
        pathIndex = 0;
    }
    
    // Exit retreat mode when healed to safe threshold (110 HP = 73%)
    // OR if stuck in retreat for too long (safety escape after 30 turns)
    bool healedEnough = (health >= RETURN_TO_COMBAT_THRESHOLD);
    bool stuckTooLong = (isRetreating && turnsWaitingForMedic >= 30);  // Escape hatch
    
    if (isRetreating && (healedEnough || stuckTooLong)) {
        isRetreating = false;
        needsHealing = false;  // Reset healing flag
        currentPath.clear();
        pathIndex = 0;
        
        if (stuckTooLong) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ⚠️ ABANDONING RETREAT - Stuck for " 
                     << turnsWaitingForMedic << " turns! HP:" << health << "/" << INITIAL_HEALTH 
                     << " - Resuming combat despite injury\n");
        } else {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] ✅ EXITING RETREAT MODE - Healed to HP:" 
                     << health << "/" << INITIAL_HEALTH << " (" << (health * 100 / INITIAL_HEALTH) 
                     << "%) - Ready for combat!\n");
        }
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
            Position medicPos = friendlyMedic->getPosition();
            
            // Retreat TOWARDS medic position for faster healing
            int targetX = medicPos.x;
            int targetY = medicPos.y;
            
            // If medic is far away, retreat halfway towards medic
            float distToMedic = position.euclideanDistance(medicPos);
            if (distToMedic > 10) {
                // Retreat halfway towards medic
                targetX = (position.x + medicPos.x) / 2;
                targetY = (position.y + medicPos.y) / 2;
            }
            
            retreatTarget = Position(targetX, targetY);
            
            // CRITICAL: Enforce map bounds safety margin (stay 2 tiles from edges)
            if (retreatTarget.x < 2) retreatTarget.x = 2;
            if (retreatTarget.y < 2) retreatTarget.y = 2;
            if (retreatTarget.x >= GRID_WIDTH - 2) retreatTarget.x = GRID_WIDTH - 3;
            if (retreatTarget.y >= GRID_HEIGHT - 2) retreatTarget.y = GRID_HEIGHT - 3;
            
            // ENHANCED: Validate retreat target with distance check (not just position equality)
            float distToTarget = position.euclideanDistance(retreatTarget);
            
            if (retreatTarget == position || distToTarget < 2.0f) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Retreat target (" 
                         << retreatTarget.x << "," << retreatTarget.y 
                         << ") too close to current position (dist: " << distToTarget 
                         << "). Finding better position...\n");
                
                // Move at least 3 tiles toward medic (or away if medic too close)
                int dx = medicPos.x - position.x;
                int dy = medicPos.y - position.y;
                float magnitude = std::sqrt(dx*dx + dy*dy);
                
                if (magnitude > 0) {
                    // Normalize and scale to at least 3 tiles (but cap at actual distance)
                    float scale = std::min(3.0f, magnitude);
                    dx = static_cast<int>((dx / magnitude) * scale);
                    dy = static_cast<int>((dy / magnitude) * scale);
                    retreatTarget = Position(position.x + dx, position.y + dy);
                    
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Adjusted retreat target to (" 
                             << retreatTarget.x << "," << retreatTarget.y << ") - " 
                             << scale << " tiles away\n");
                } else {
                    // Medic at same position? Just pick a direction
                    retreatTarget = Position(position.x + 3, position.y);
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Medic at same position! Retreating arbitrarily.\n");
                }
            }
        }
        lastUpdateTurn = currentTurn;
    }
    
    // Defensive shooting - only at close enemies (within 4 tiles)
    Character* visibleEnemy = findNearestEnemy(allCharacters);
    if (visibleEnemy && position.euclideanDistance(visibleEnemy->getPosition()) <= 4) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Defensive shot while retreating\n");
        tryShootEnemy(visibleEnemy, map);
    }
    
    // Check if medic is coming to help
    Character* friendlyMedic = nullptr;
    float distToMedic = 999.0f;
    
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::MEDIC) {
            friendlyMedic = c;
            distToMedic = position.euclideanDistance(c->getPosition());
            break;
        }
    }
    
    // ENHANCED LAST STAND: Trigger if medic doesn't exist OR if medic is stuck
    if (!friendlyMedic) {
        // Original logic: No medic exists at all
        turnsWaitingForMedic++;
        
        if (turnsWaitingForMedic > 10) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                     << "] No medic available for " << turnsWaitingForMedic 
                     << " turns. Canceling retreat - LAST STAND MODE!\n");
            
            isRetreating = false;
            needsHealing = false;  // Accept our fate
            autonomousCooldown = 100;  // FIX 3: Cap at 100 (was 999) - fight independently until death
            turnsWaitingForMedic = 0;
            turnsWithoutMedicProgress = 0;
            lastKnownMedicDistance = 999.0f;
            lastStandCooldown = 20;  // FIX 1: Prevent re-entry to retreat for 20 turns
            currentPath.clear();
            pathIndex = 0;
            return;
        }
        
        LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                 << "] No medic available (" << turnsWaitingForMedic << "/10 turns). Will enter Last Stand soon.\n");
    } else {
        // NEW LOGIC: Medic exists, but is it making progress toward us?
        
        // Check if medic got closer since last turn (tolerance: 0.5 tiles)
        bool medicMakingProgress = (distToMedic < lastKnownMedicDistance - 0.5f);
        
        // Only track "stuck medic" if medic is far away (>5 tiles)
        if (!medicMakingProgress && distToMedic > 5.0f) {
            // Medic is far away AND not getting closer = probably stuck or blocked
            turnsWithoutMedicProgress++;
            
            LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                     << "] Medic not making progress (dist: " << distToMedic 
                     << " tiles, stuck for " << turnsWithoutMedicProgress << "/8 turns)\n");
            
            // LAST STAND: If medic hasn't made progress for 8+ turns, assume it's blocked
            if (turnsWithoutMedicProgress >= 8) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                         << "] Medic stuck/unreachable for " << turnsWithoutMedicProgress 
                         << " turns (dist: " << distToMedic << " tiles). Entering LAST STAND MODE!\n");
                
                isRetreating = false;
                needsHealing = false;
                autonomousCooldown = 100;  // FIX 3: Cap at 100 (was 999) - fight independently
                turnsWaitingForMedic = 0;
                turnsWithoutMedicProgress = 0;
                lastKnownMedicDistance = 999.0f;
                lastStandCooldown = 20;  // FIX 1: Prevent re-entry to retreat for 20 turns
                currentPath.clear();
                pathIndex = 0;
                return;
            }
        } else {
            // Medic is getting closer or already close - reset stuck counter
            turnsWithoutMedicProgress = 0;
            turnsWaitingForMedic = 0;  // Also reset "no medic" counter
        }
        
        // Update last known distance for next turn comparison
        lastKnownMedicDistance = distToMedic;
        
        // CRITICAL FIX: Explicitly check for nearby enemies in retreat mode
        // visibleEnemy may be NULL here, so we need to scan for threats
        float distToEnemy = 999.0f;
        Character* nearestEnemy = nullptr;
        
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                float dist = position.euclideanDistance(c->getPosition());
                if (dist < distToEnemy) {
                    distToEnemy = dist;
                    nearestEnemy = c;
                }
            }
        }
        
        // Log enemy proximity for debugging
        if (nearestEnemy && distToEnemy <= 10) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Enemy detected at " 
                     << distToEnemy << " tiles during retreat\n");
        }
        
        // Continue with existing wait/move logic based on whether medic is approaching
        // Only wait for medic if:
        // 1. Medic is within reasonable distance (15 tiles)
        // 2. No enemies are very close (>6 tiles) OR we're already very low health (<20)
        // 3. CRITICAL: Medic is actually making progress (not stuck)
        // If enemies are close and we have some health, keep retreating to create distance
        bool medicApproaching = (turnsWithoutMedicProgress < 3);  // Medic made progress in last 3 turns
        bool shouldWaitForMedic = (distToMedic <= 15) && 
                                   (distToEnemy > 6 || health < 20) &&
                                   medicApproaching;  // NEW: Don't wait if medic is stuck!
        
        if (shouldWaitForMedic) {
            turnsWaitingForMedic++;
            
            // If we've waited for 5+ turns, move toward medic instead
            if (turnsWaitingForMedic >= 5) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Waited " << turnsWaitingForMedic 
                         << " turns for medic. Moving toward medic instead!\n");
                
                // Set retreat target to medic's position
                retreatTarget = friendlyMedic->getPosition();
                currentPath.clear();
                pathIndex = 0;
                turnsWaitingForMedic = 0;  // Reset counter
                // Don't return - let the pathfinding below handle movement
            } else {
                // ADDITIONAL SAFETY: If enemy is dangerously close (<=4 tiles), reposition away
                // instead of holding position, even when waiting for medic
                if (nearestEnemy && distToEnemy <= 4) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Enemy too close (" 
                             << distToEnemy << " tiles) while waiting for medic. Repositioning away!\n");
                    
                    // Move away from enemy toward a safer position
                    Position enemyPos = nearestEnemy->getPosition();
                    int dx = position.x - enemyPos.x;
                    int dy = position.y - enemyPos.y;
                    
                    // Normalize direction away from enemy
                    float magnitude = std::sqrt(dx*dx + dy*dy);
                    if (magnitude > 0.1f) {
                        dx = static_cast<int>(dx / magnitude * 5);  // Move 5 tiles away
                        dy = static_cast<int>(dy / magnitude * 5);
                    } else {
                        dx = 5; dy = 0;  // Default direction if on same spot
                    }
                    
                    Position safePos(position.x + dx, position.y + dy);
                    if (isValidPosition(safePos) && map.isPassable(safePos)) {
                        retreatTarget = safePos;
                    } else {
                        // Try to find a valid position nearby
                        for (int offset = 3; offset <= 7; ++offset) {
                            int nx = position.x - (enemyPos.x - position.x > 0 ? offset : -offset);
                            int ny = position.y - (enemyPos.y - position.y > 0 ? offset : -offset);
                            Position altPos(nx, ny);
                            if (isValidPosition(altPos) && map.isPassable(altPos)) {
                                retreatTarget = altPos;
                                break;
                            }
                        }
                    }
                    
                    currentPath.clear();
                    pathIndex = 0;
                    // Continue to pathfinding below
                } else {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Medic approaching (" 
                             << distToMedic << " tiles away), holding position for healing (waited " 
                             << turnsWaitingForMedic << " turns)\n");
                    currentPath.clear();
                    pathIndex = 0;
                    return;  // Don't move, let medic come to us
                }
            }
        } else {
            // Not waiting - either medic too far, enemy too close, or medic is stuck
            turnsWaitingForMedic = 0;
            
            // CRITICAL: If medic is stuck (not making progress for 3+ turns), move toward medic!
            if (distToMedic <= 15 && turnsWithoutMedicProgress >= 3) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Medic stuck for " 
                         << turnsWithoutMedicProgress << " turns at " << distToMedic 
                         << " tiles away. Moving toward medic to close distance!\n");
                
                // Update retreat target to medic's CURRENT position
                retreatTarget = friendlyMedic->getPosition();
                currentPath.clear();
                pathIndex = 0;
                // Don't return - continue to pathfinding logic below
            }
        }
    }
    
    // Move towards retreat target if medic is far away
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
        
        // CRITICAL FIX A: Validate path direction - ensure it moves TOWARD target, not away
        if (!currentPath.empty() && currentPath.size() > 1) {
            Position nextStep = currentPath[1];  // First actual move (index 0 is current position)
            float distBefore = position.euclideanDistance(retreatTarget);
            float distAfter = nextStep.euclideanDistance(retreatTarget);
            
            // If first step INCREASES distance to retreat target, path is going wrong direction!
            if (distAfter > distBefore + 0.5f) {  // +0.5 tolerance for diagonal moves
                LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                         << "] Retreat path moves AWAY from target! (dist: " << distBefore 
                         << " -> " << distAfter << "). Trying direct path...\n");
                
                // Try direct path without safety considerations (survival mode - get to medic fast)
                currentPath = AI::findPath(position, retreatTarget, map);
                pathIndex = 0;
                
                // If direct path also fails or goes wrong direction, mark this in logs
                if (currentPath.empty() || currentPath.size() <= 1) {
                    LOG_CHARACTER("[WARRIOR " << teamToString(team) 
                             << "] Direct retreat path also failed! Entering fallback mode.\n");
                }
            }
        }
        
        // ENHANCED: Check if path is empty OR too short (single-cell paths are useless)
        if (currentPath.empty() || currentPath.size() <= 1) {
            LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Cannot path to retreat target at (" 
                     << retreatTarget.x << "," << retreatTarget.y << ") - path size: " 
                     << currentPath.size() << ". Trying fallback positions...\n");
            
            // FALLBACK 1: Try positions AROUND retreat target (adjacent cells)
            const std::vector<Position> adjacentOffsets = {
                {-1, -1}, {0, -1}, {1, -1},
                {-1,  0},          {1,  0},
                {-1,  1}, {0,  1}, {1,  1}
            };
            
            bool foundFallback = false;
            for (const auto& offset : adjacentOffsets) {
                Position fallback(retreatTarget.x + offset.x, retreatTarget.y + offset.y);
                if (map.isPassable(fallback)) {
                    currentPath = AI::findPath(position, fallback, map, &safetyMap, 2.0f);
                    if (!currentPath.empty() && currentPath.size() > 1) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Found fallback retreat position at (" 
                                 << fallback.x << "," << fallback.y << ") with path size: " 
                                 << currentPath.size() << "\n");
                        pathIndex = 0;
                        foundFallback = true;
                        break;
                    }
                }
            }
            
            // FALLBACK 2: Retreat in OPPOSITE direction from nearest enemy
            if (!foundFallback) {
                Character* nearestThreat = findNearestEnemy(allCharacters);
                if (nearestThreat) {
                    Position threatPos = nearestThreat->getPosition();
                    int dx = position.x - threatPos.x;  // Away from enemy
                    int dy = position.y - threatPos.y;
                    
                    // Normalize and extend
                    float magnitude = std::sqrt(dx*dx + dy*dy);
                    if (magnitude > 0) {
                        dx = static_cast<int>((dx / magnitude) * 5);  // Move 5 tiles away
                        dy = static_cast<int>((dy / magnitude) * 5);
                    }
                    
                    Position escapePos(position.x + dx, position.y + dy);
                    currentPath = AI::findPath(position, escapePos, map, &safetyMap, 2.0f);
                    
                    if (!currentPath.empty() && currentPath.size() > 1) {
                        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] Retreating away from threat at (" 
                                 << threatPos.x << "," << threatPos.y << ") toward (" 
                                 << escapePos.x << "," << escapePos.y << ")\n");
                        pathIndex = 0;
                        foundFallback = true;
                    }
                }
            }
            
            // LAST RESORT: Hold position in defensive stance
            if (!foundFallback) {
                LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] TRAPPED! All retreat paths blocked. Fighting defensively.\n");
                currentPath.clear();
                pathIndex = 0;
                // Stay in retreat mode but fight back aggressively
                // Will shoot at enemies in executeRetreat defensive section
            }
        }
    }
    
    // RETREAT SPRINT MECHANIC: On first turn of retreat, move 3 tiles immediately to escape kill zone
    if (retreatSprintActive) {
        LOG_CHARACTER("[WARRIOR " << teamToString(team) << "] 🏃💨 RETREAT SPRINT! Moving 3 tiles to escape kill zone!\n");
        
        // Move 3 times in rapid succession
        for (int sprint = 0; sprint < 3; ++sprint) {
            if (!currentPath.empty() && pathIndex < currentPath.size() - 1) {
                moveAlongPath(allCharacters);
            } else {
                // Path exhausted, break early
                break;
            }
        }
        
        retreatSprintActive = false;  // Disable sprint after first turn
        return;  // Already moved, don't move again this turn
    }
    
    // Move along retreat path (normal 1 tile/turn movement)
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
 * @brief Take damage and track timing for tactical withdrawal
 */
void Warrior::takeDamage(int damage) {
    Character::takeDamage(damage);  // Call base class implementation
    // Note: Don't pass currentTurn here - we'll track in update() instead
    // This keeps takeDamage simple and avoids needing to pass currentTurn everywhere
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

/**
 * @brief PRIORITY 2: Calculate team center position (centroid of all team warriors)
 */
Position Warrior::calculateTeamCenter(const std::vector<Character*>& allCharacters) const {
    int sumX = 0, sumY = 0;
    int count = 0;
    
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::WARRIOR) {
            Position pos = c->getPosition();
            sumX += pos.x;
            sumY += pos.y;
            count++;
        }
    }
    
    if (count == 0) return position;  // Fallback to self if no team found
    
    return Position(sumX / count, sumY / count);
}

/**
 * @brief PRIORITY 2: Check if warrior is too far from team (anti-separation)
 * Returns true if warrior is >18 tiles from team center
 */
bool Warrior::isTooFarFromTeam(const std::vector<Character*>& allCharacters) const {
    Position teamCenter = calculateTeamCenter(allCharacters);
    float distanceFromTeam = position.euclideanDistance(teamCenter);
    
    constexpr float MAX_TEAM_SPREAD = 18.0f;  // Maximum allowed distance from team center
    
    return distanceFromTeam > MAX_TEAM_SPREAD;
}
