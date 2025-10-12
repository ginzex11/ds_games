#include "supplier.h"
#include "warrior.h"

/**
 * @brief Construct a new Supplier
 */
Supplier::Supplier(Position pos, Team t)
    : Character(pos, t, CharacterType::SUPPLIER),
      ammoSupplies(3), currentRecipient(nullptr), returningFromWarehouse(false) {
}

/**
 * @brief Update supplier - resupply warriors or get ammo
 */
void Supplier::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
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
        } else {
            // Check if close enough to recipient (within 2 cells - manhattan distance)
            float distance = position.manhattanDistance(currentRecipient->getPosition());
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Distance to recipient: " << distance 
                     << " | Has ammo: " << (hasAmmo() ? "Yes" : "No")
                     << " | Supplies: " << ammoSupplies << "\n");
            
            if (distance <= 2) {
                // Close enough - can resupply (allows diagonal adjacency)
                if (hasAmmo()) {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Resupplying recipient!\n");
                    resupplyRecipient();
                } else {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] No ammo, going to warehouse\n");
                    travelToWarehouse(map, safetyMap);
                }
            } else if (!hasAmmo() && !returningFromWarehouse) {
                // Need to get ammo first
                LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Going to warehouse for supplies\n");
                travelToWarehouse(map, safetyMap);
            } else {
                // Have ammo or returning, move towards recipient
                Position recipientPos = currentRecipient->getPosition();
                
                // Update path if recipient has moved significantly
                // or if we have no path or path is nearly complete
                bool needsNewPath = currentPath.empty() || 
                                   pathIndex >= static_cast<int>(currentPath.size()) - 2;
                
                // Check if recipient moved from our target
                if (!needsNewPath && !currentPath.empty()) {
                    Position currentTarget = currentPath[currentPath.size() - 1];
                    if (currentTarget != recipientPos) {
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
            }
        }
    }
    
    // Move along path
    moveAlongPath(allCharacters);
    
    // Check if reached warehouse
    if (map.isWarehouse(position) && 
        map.getWarehouseType(position) == WarehouseType::AMMO) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Reached warehouse, collecting ammo\n");
        collectAmmo();
        returningFromWarehouse = true;
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
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Calculating path to warehouse at (" 
                 << warehouse.x << "," << warehouse.y << ") using safety map\n");
        // Use lower safety weight for support units to ensure they can reach warehouses
        currentPath = AI::findPath(position, warehouse, map, &safetyMap, 0.1f);
        
        // Fallback: If no safe path, try direct path (suppliers must reach warehouse!)
        if (currentPath.empty()) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] No safe path, trying direct route\n");
            currentPath = AI::findPath(position, warehouse, map);
        }
        
        pathIndex = 0;
        
        if (currentPath.empty()) {
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] WARNING: No path to warehouse found!\n");
        }
    }
}

/**
 * @brief Navigate to recipient using safety map
 */
void Supplier::travelToRecipient(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    if (!currentRecipient) return;
    
    Position recipientPos = currentRecipient->getPosition();
    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Calculating path to recipient at (" 
             << recipientPos.x << "," << recipientPos.y << ") using safety map\n");
    // Use lower safety weight for support units to ensure they can reach targets
    currentPath = AI::findPath(position, recipientPos, map, &safetyMap, 0.1f);
    
    // Fallback: If no safe path, try direct path (suppliers must reach warriors!)
    if (currentPath.empty()) {
        LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] No safe path, trying direct route\n");
        currentPath = AI::findPath(position, recipientPos, map);
    }
    
    pathIndex = 0;
}

/**
 * @brief Resupply the current recipient
 */
void Supplier::resupplyRecipient() {
    if (!currentRecipient || ammoSupplies <= 0) return;
    
    // Resupply warrior
    Warrior* warrior = dynamic_cast<Warrior*>(currentRecipient);
    if (warrior) {
        warrior->resupplyAmmo(WAREHOUSE_RESUPPLY_AMOUNT);
        ammoSupplies--;
        
        // Clear order
        currentRecipient = nullptr;
        currentOrder = Order();
        returningFromWarehouse = false;
    }
}
