#include "supplier.h"
#include "warrior.h"

/**
 * @brief Construct a new Supplier
 */
Supplier::Supplier(Position pos, Team t)
    : Character(pos, t, CharacterType::SUPPLIER),
      ammoSupplies(0), currentRecipient(nullptr), returningFromWarehouse(false),
      recipientPathFailures(0), lastRecipientPathAttempt(0), currentTurnTracker(0) {
    // Suppliers start with 0 ammo - must receive order from commander to get supplies
}

/**
 * @brief Update supplier - resupply warriors or get ammo
 */
void Supplier::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Track current turn for timeout logic
    currentTurnTracker = currentTurn;
    
    // YIELD LOGIC: If standing still and blocking friendly warriors, move aside
    if (currentPath.empty() || pathIndex >= static_cast<int>(currentPath.size())) {
        // Check if any friendly warriors want this position
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() == team && c->getType() == CharacterType::WARRIOR) {
                // Check if warrior's next move is to our position
                const auto& path = c->getCurrentPath();
                int idx = c->getPathIndex();
                if (!path.empty() && idx < static_cast<int>(path.size())) {
                    Position warriorNextPos = path[idx];
                    if (warriorNextPos == position) {
                        // Warrior wants our position - move to adjacent free cell
                        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Yielding position to warrior\n");
                        // Check all 4 adjacent cells
                        std::vector<Position> neighbors = {
                            Position(position.x + 1, position.y),
                            Position(position.x - 1, position.y),
                            Position(position.x, position.y + 1),
                            Position(position.x, position.y - 1)
                        };
                        for (const Position& neighbor : neighbors) {
                            if (isValidPosition(neighbor) && map.isPassable(neighbor)) {
                                // Check if neighbor is not occupied
                                bool neighborFree = true;
                                for (Character* other : allCharacters) {
                                    if (other != this && other->isAlive() && other->getPosition() == neighbor) {
                                        neighborFree = false;
                                        break;
                                    }
                                }
                                if (neighborFree) {
                                    currentPath = {neighbor};
                                    pathIndex = 0;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Update visibility
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
    // Generate safety map based on visible enemies
    std::vector<Position> enemyPositions;
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() != team) {
            enemyPositions.push_back(c->getPosition());
        }
    }
    auto safetyMap = AI::generateSafetyMap(enemyPositions, map);
    
    // AUTONOMOUS RESUPPLY: If idle and low on ammo, go to warehouse
    if (currentOrder.type != OrderType::RESUPPLY && ammoSupplies <= 1 && !returningFromWarehouse) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] AUTONOMOUS: Low on supplies (" 
                 << ammoSupplies << " packs), going to warehouse\n");
        travelToWarehouse(map, safetyMap);
        moveAlongPath(allCharacters);
        
        // Check if reached warehouse
        if (map.isWarehouse(position) && map.getWarehouseType(position) == WarehouseType::AMMO) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Reached warehouse during autonomous resupply\n");
            Map& mutableMap = const_cast<Map&>(map);
            collectAmmo(mutableMap);
            currentPath.clear();
            pathIndex = 0;
        }
        return;  // Skip normal order processing this turn
    }
    
    // If carrying out resupply order
    if (currentOrder.type == OrderType::RESUPPLY && currentRecipient) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Executing RESUPPLY order for " 
                 << teamToString(currentRecipient->getTeam()) << " recipient at (" 
                 << currentRecipient->getPosition().x << "," << currentRecipient->getPosition().y << ")\n");
        
        // SAFETY CHECK: Never resupply enemy team!
        if (currentRecipient->getTeam() != team) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] ERROR: Assigned to resupply ENEMY! Clearing order.\n");
            currentRecipient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
            return;
        }
        
        if (!currentRecipient->isAlive()) {
            // Recipient died, clear order and stop immediately
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Recipient died, clearing order\n");
            currentRecipient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
            currentPath.clear();  // Clear path to stop moving
            pathIndex = 0;
            return;  // Stop execution immediately
        }
        
        // PROACTIVE: If no ammo, get supplies FIRST before traveling to recipient
        if (!hasAmmo() && !returningFromWarehouse) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] No ammo! Going to warehouse before recipient\n");
            travelToWarehouse(map, safetyMap);
            // Let moveAlongPath() execute below to actually move towards warehouse
        } else {
            // Have ammo or returning from warehouse - now handle recipient
            // Check if close enough to recipient (within 2 cells - manhattan distance)
            float distance = position.manhattanDistance(currentRecipient->getPosition());
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Distance to recipient: " << distance 
                     << " | Has ammo: " << (hasAmmo() ? "Yes" : "No")
                     << " | Supplies: " << ammoSupplies << "\n");
            
            if (distance <= 2) {
                // Close enough - can resupply (allows diagonal adjacency)
                if (hasAmmo()) {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Resupplying recipient!\n");
                    Map& mutableMap = const_cast<Map&>(map);
                    resupplyRecipient(mutableMap);
                } else {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] At recipient but no ammo, going to warehouse\n");
                    travelToWarehouse(map, safetyMap);
                }
            } else {
                // Have ammo or returning, move towards recipient
                Position recipientPos = currentRecipient->getPosition();
                
                // OPTION C: Emergency sprint if warrior has 0 ammo (critical situation)
                bool emergencyMode = false;
                if (Warrior* w = dynamic_cast<Warrior*>(currentRecipient)) {
                    if (w->getAmmo() == 0) {
                        emergencyMode = true;
                        LOG_CHARACTER("[SUPPLIER " << teamToString(team) 
                                 << "] EMERGENCY MODE: Warrior has 0 ammo! Double speed to recipient\n");
                    }
                }
                
                // Update path if recipient has moved significantly
                // or if we have no path or path is nearly complete
                bool needsNewPath = currentPath.empty() || 
                                   pathIndex >= static_cast<int>(currentPath.size()) - 2;
                
                // Check if recipient moved from our target
                // FIX: Allow target to be adjacent to recipient (for blocked paths)
                if (!needsNewPath && !currentPath.empty()) {
                    Position currentTarget = currentPath[currentPath.size() - 1];
                    float distToTarget = currentTarget.euclideanDistance(recipientPos);
                    // Recipient moved if target is more than 1.5 tiles away (allows adjacent/diagonal)
                    if (distToTarget > 1.5f) {
                        needsNewPath = true;
                        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Recipient moved! Recalculating path\n");
                    }
                }
                
                if (needsNewPath) {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Moving towards recipient at (" 
                             << recipientPos.x << "," << recipientPos.y << ")"
                             << " | Recalculating path\n");
                    travelToRecipient(map, safetyMap);
                } else {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Following existing path to recipient"
                             << " | Path size: " << currentPath.size() << " | PathIndex: " << pathIndex << "\n");
                }
                
                // OPTION C: Double speed movement in emergency mode
                if (emergencyMode) {
                    moveAlongPath(allCharacters);  // Move twice per turn
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Emergency sprint: moved 2 steps\n");
                }
            }
        }
    }
    
    // Move along path (normal speed)
    moveAlongPath(allCharacters);
    
    // Check if reached warehouse (only trigger ONCE when first arriving)
    if (map.isWarehouse(position) && 
        map.getWarehouseType(position) == WarehouseType::AMMO &&
        !returningFromWarehouse) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Reached warehouse, collecting ammo\n");
        
        // Need mutable copy of map
        Map& mutableMap = const_cast<Map&>(map);
        collectAmmo(mutableMap);
        returningFromWarehouse = true;
        
        // Clear current path to force recalculation
        currentPath.clear();
        pathIndex = 0;
        
        // Immediately set path to recipient
        if (currentRecipient) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Got supplies! Heading to recipient now\n");
            travelToRecipient(map, safetyMap);
            
            // CRITICAL: Skip first position if it's our current position (warehouse)
            // This ensures we actually LEAVE the warehouse instead of staying stuck
            if (!currentPath.empty() && currentPath[0] == position) {
                LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Skipping warehouse position in path\n");
                pathIndex = 1;  // Start from next position
            }
        }
    }
}

/**
 * @brief Execute resupply order
 */
void Supplier::executeOrder(Order order, const Map& map) {
    currentOrder = order;
    
    if (order.type == OrderType::RESUPPLY) {
        currentRecipient = order.targetCharacter;
        returningFromWarehouse = false;
        
        // PROACTIVE SUPPLY MANAGEMENT: If no ammo, go to warehouse immediately
        // This allows supplier to prepare BEFORE warrior calls for resupply
        if (!hasAmmo()) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) 
                     << "] PROACTIVE: Received resupply order but no supplies. Going to warehouse first!\n");
            // Path to warehouse will be calculated in update() with safety map
        }
        // Path will be calculated in update() with safety map
    } else if (order.type == OrderType::MOVE) {
        currentPath = AI::findPath(position, order.targetPosition, map);
        pathIndex = 0;
    }
}

/**
 * @brief Navigate to ammo warehouse using safety map
 */
void Supplier::travelToWarehouse(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    Position warehouse = map.getWarehouse(team, WarehouseType::AMMO);
    
    // Only recalculate if we don't have a path or it's been cleared
    if (currentPath.empty() || pathIndex >= static_cast<int>(currentPath.size())) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Calculating DIRECT path to warehouse at (" 
                 << warehouse.x << "," << warehouse.y << ") - ignoring safety\n");
        
        // Track consecutive pathfinding failures for last-resort teleport
        static int warehousePathFailures = 0;
        
        // CRITICAL: Support units MUST reach warehouses - use direct path with NO safety weight
        currentPath = AI::findPath(position, warehouse, map);
        
        pathIndex = 0;
        
        if (currentPath.empty()) {
            warehousePathFailures++;
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] ERROR: No path to warehouse found! (Failure #" 
                     << warehousePathFailures << ") Current pos: (" << position.x << "," << position.y << ")\n");
            
            // Try nearby positions if exact warehouse position is blocked
            std::vector<Position> nearbyPositions = {
                Position(warehouse.x + 1, warehouse.y),
                Position(warehouse.x - 1, warehouse.y),
                Position(warehouse.x, warehouse.y + 1),
                Position(warehouse.x, warehouse.y - 1),
                Position(warehouse.x + 1, warehouse.y + 1),
                Position(warehouse.x - 1, warehouse.y - 1),
                Position(warehouse.x + 1, warehouse.y - 1),
                Position(warehouse.x - 1, warehouse.y + 1)
            };
            
            for (const Position& nearbyPos : nearbyPositions) {
                if (nearbyPos.x >= 0 && nearbyPos.x < GRID_WIDTH && 
                    nearbyPos.y >= 0 && nearbyPos.y < GRID_HEIGHT &&
                    map.isPassable(nearbyPos)) {
                    currentPath = AI::findPath(position, nearbyPos, map);
                    if (!currentPath.empty()) {
                        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Found alternate path to nearby position (" 
                                 << nearbyPos.x << "," << nearbyPos.y << ")\n");
                        warehousePathFailures = 0; // Reset on success
                        break;
                    }
                }
            }
            
            // LAST RESORT: If supplier is completely trapped for 5+ turns, teleport near warehouse
            if (currentPath.empty() && warehousePathFailures >= 5) {
                LOG_CHARACTER("[SUPPLIER " << teamToString(team) 
                         << "] CRITICAL: Completely trapped after " << warehousePathFailures 
                         << " failures! EMERGENCY TELEPORT to warehouse area\n");
                
                // Find nearest passable position to warehouse
                bool teleported = false;
                for (int radius = 1; radius <= 5 && !teleported; ++radius) {
                    for (int dy = -radius; dy <= radius && !teleported; ++dy) {
                        for (int dx = -radius; dx <= radius && !teleported; ++dx) {
                            Position teleportPos(warehouse.x + dx, warehouse.y + dy);
                            if (isValidPosition(teleportPos) && map.isPassable(teleportPos)) {
                                position = teleportPos;
                                warehousePathFailures = 0;
                                teleported = true;
                                LOG_CHARACTER("[SUPPLIER " << teamToString(team) 
                                         << "] Emergency teleported to (" << position.x << "," << position.y 
                                         << ") - " << radius << " tiles from warehouse\n");
                            }
                        }
                    }
                }
            }
        } else {
            warehousePathFailures = 0; // Reset on successful path
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Warehouse path: " << currentPath.size() << " cells\n");
        }
    }
}

/**
 * @brief Navigate to recipient using safety map
 */
void Supplier::travelToRecipient(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    if (!currentRecipient) return;
    
    Position recipientPos = currentRecipient->getPosition();
    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Calculating DIRECT path to recipient at (" 
             << recipientPos.x << "," << recipientPos.y << ") - prioritizing speed\n");
    
    // CRITICAL: Support units MUST reach warriors - use direct path with NO safety weight
    currentPath = AI::findPath(position, recipientPos, map);
    
    pathIndex = 0;
    
    if (currentPath.empty()) {
        // Track pathfinding failures
        recipientPathFailures++;
        
        // TIMEOUT LOGIC: After 10 consecutive failures, give up on this recipient
        if (recipientPathFailures >= 10) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] ERROR: Failed to path to recipient " 
                     << recipientPathFailures << " times. Clearing order - recipient unreachable!\n");
            currentRecipient = nullptr;
            currentOrder = Order();
            recipientPathFailures = 0;
            lastRecipientPathAttempt = currentTurnTracker;
            return;
        }
        
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] ERROR: No path to recipient found (failure " 
                 << recipientPathFailures << "/10)! Will retry next turn.\n");
    } else {
        // Success! Reset failure counter
        recipientPathFailures = 0;
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Recipient path: " << currentPath.size() << " cells\n");
    }
}

/**
 * @brief Resupply the current recipient
 */
void Supplier::resupplyRecipient(Map& map) {
    if (!currentRecipient || ammoSupplies <= 0) return;
    
    // Resupply warrior
    Warrior* warrior = dynamic_cast<Warrior*>(currentRecipient);
    if (warrior) {
        // Calculate how much warrior needs
        int ammoNeeded = warrior->getAmmoNeeded();
        int grenadesNeeded = warrior->getGrenadesNeeded();
        
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Warrior needs: " 
                 << ammoNeeded << " ammo, " << grenadesNeeded << " grenades\n");
        
        // Try to take from warehouse inventory
        int ammoToGive = 0;
        int grenadesToGive = 0;
        
        if (map.takeAmmo(team, ammoNeeded)) {
            ammoToGive = ammoNeeded;
        } else {
            // Give whatever is available
            int available = map.getAmmoInventory(team);
            if (map.takeAmmo(team, available)) {
                ammoToGive = available;
            }
        }
        
        if (map.takeGrenades(team, grenadesNeeded)) {
            grenadesToGive = grenadesNeeded;
        } else {
            // Give whatever is available
            int available = map.getGrenadeInventory(team);
            if (map.takeGrenades(team, available)) {
                grenadesToGive = available;
            }
        }
        
        // Resupply warrior with what we got
        warrior->resupplyAmmo(ammoToGive, grenadesToGive);
        ammoSupplies--;  // Used one supply pack
        
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Gave " << ammoToGive 
                 << " ammo, " << grenadesToGive << " grenades. Warehouse now has: " 
                 << map.getAmmoInventory(team) << " ammo, " 
                 << map.getGrenadeInventory(team) << " grenades\n");
        
        // Clear order
        currentRecipient = nullptr;
        currentOrder = Order();
        returningFromWarehouse = false;
    }
}

/**
 * @brief Collect ammo supplies from warehouse
 */
void Supplier::collectAmmo(Map& map) {
    // Supplier just picks up a "supply pack" - doesn't take specific amounts yet
    ammoSupplies++;
    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Collected supply pack (now has " 
             << ammoSupplies << " packs). Warehouse inventory: " 
             << map.getAmmoInventory(team) << " ammo, " 
             << map.getGrenadeInventory(team) << " grenades\n");
}
