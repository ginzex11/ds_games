#include "medic.h"
#include "warrior.h"

/**
 * @brief Construct a new Medic
 */
Medic::Medic(Position pos, Team t)
    : Character(pos, t, CharacterType::MEDIC),
      medicineSupplies(0), currentPatient(nullptr), returningFromWarehouse(false) {
    // Medics must visit warehouse FIRST to get supplies before healing
}

/**
 * @brief Update medic - heal warriors or get supplies
 */
void Medic::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
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
                        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Yielding position to warrior\n");
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
    
    // If carrying out heal order
    if (currentOrder.type == OrderType::HEAL && currentPatient) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Executing HEAL order for " 
                 << teamToString(currentPatient->getTeam()) << " patient at (" 
                 << currentPatient->getPosition().x << "," << currentPatient->getPosition().y << ")\n");
        
        // SAFETY CHECK: Never heal enemy team!
        if (currentPatient->getTeam() != team) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] ERROR: Assigned to heal ENEMY! Clearing order.\n");
            currentPatient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
            return;
        }
        
        if (!currentPatient->isAlive()) {
            // Patient died, clear order and stop immediately
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Patient died, clearing order\n");
            currentPatient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
            currentPath.clear();  // Clear path to stop moving
            pathIndex = 0;
            return;  // Stop execution immediately
        }
        
        // PROACTIVE: If no medicine, get supplies FIRST before traveling to patient
        if (!hasMedicine() && !returningFromWarehouse) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] No medicine! Going to warehouse before patient\n");
            travelToWarehouse(map, safetyMap);
            // Let moveAlongPath() execute below to actually move towards warehouse
        } else {
            // Have medicine or returning from warehouse - now handle patient
            // Check if close enough to patient (within 2 cells - manhattan distance)
            float distance = position.manhattanDistance(currentPatient->getPosition());
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Distance to patient: " << distance 
                     << " | Has medicine: " << (hasMedicine() ? "Yes" : "No") 
                     << " | Supplies: " << medicineSupplies << "\n");
            
            if (distance <= 2) {
                // Close enough - can heal (allows diagonal adjacency)
                if (hasMedicine()) {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Healing patient!\n");
                    Map& mutableMap = const_cast<Map&>(map);
                    healPatient(mutableMap);
                } else {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] At patient but no medicine, going to warehouse\n");
                    travelToWarehouse(map, safetyMap);
                }
            } else {
                // Have medicine or returning, move towards patient
                Position patientPos = currentPatient->getPosition();
                
                // Update path if patient has moved significantly (retreating warriors move!)
                // or if we have no path or path is nearly complete
                bool needsNewPath = currentPath.empty() || 
                                   pathIndex >= static_cast<int>(currentPath.size()) - 2;
                
                // Check if patient moved from our target (happens during retreat)
                if (!needsNewPath && !currentPath.empty()) {
                    Position currentTarget = currentPath[currentPath.size() - 1];
                    if (currentTarget != patientPos) {
                        needsNewPath = true;
                        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Patient moved! Recalculating path\n");
                    }
                }
                
                if (needsNewPath) {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Moving towards patient at (" 
                             << patientPos.x << "," << patientPos.y << ")"
                             << " | Recalculating path\n");
                    travelToPatient(map, safetyMap);
                } else {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Following existing path to patient"
                             << " | Path size: " << currentPath.size() << " | PathIndex: " << pathIndex << "\n");
                }
            }
        }
    }
    
    // Move along path
    moveAlongPath(allCharacters);
    
    // Check if reached warehouse (only trigger ONCE when first arriving)
    if (map.isWarehouse(position) && 
        map.getWarehouseType(position) == WarehouseType::MEDICINE &&
        !returningFromWarehouse) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Reached warehouse, collecting medicine\n");
        
        // Need mutable copy of map
        Map& mutableMap = const_cast<Map&>(map);
        collectMedicine(mutableMap);
        returningFromWarehouse = true;
        
        // Clear current path to force recalculation
        currentPath.clear();
        pathIndex = 0;
        
        // Immediately set path to patient
        if (currentPatient) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Got supplies! Heading to patient now\n");
            travelToPatient(map, safetyMap);
            
            // CRITICAL: Skip first position if it's our current position (warehouse)
            // This ensures we actually LEAVE the warehouse instead of staying stuck
            if (!currentPath.empty() && currentPath[0] == position) {
                LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Skipping warehouse position in path\n");
                pathIndex = 1;  // Start from next position
            }
        }
    }
}

/**
 * @brief Execute heal order
 */
void Medic::executeOrder(Order order, const Map& map) {
    currentOrder = order;
    
    if (order.type == OrderType::HEAL) {
        currentPatient = order.targetCharacter;
        returningFromWarehouse = false;
        
        // PROACTIVE SUPPLY MANAGEMENT: If no medicine, go to warehouse immediately
        // This allows medic to prepare BEFORE warrior retreats all the way back
        if (!hasMedicine()) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) 
                     << "] PROACTIVE: Received heal order but no supplies. Going to warehouse first!\n");
            // Path to warehouse will be calculated in update() with safety map
        }
        // Path will be calculated in update() with safety map
    } else if (order.type == OrderType::MOVE) {
        currentPath = AI::findPath(position, order.targetPosition, map);
        pathIndex = 0;
    }
}

/**
 * @brief Navigate to medicine warehouse using safety map
 */
void Medic::travelToWarehouse(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    Position warehouse = map.getWarehouse(team, WarehouseType::MEDICINE);
    
    // Only recalculate if we don't have a path or it's been cleared
    if (currentPath.empty() || pathIndex >= static_cast<int>(currentPath.size())) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Calculating DIRECT path to warehouse at (" 
                 << warehouse.x << "," << warehouse.y << ") - ignoring safety\n");
        
        // CRITICAL: Support units MUST reach warehouses - use direct path with NO safety weight
        currentPath = AI::findPath(position, warehouse, map);
        
        pathIndex = 0;
        
        if (currentPath.empty()) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] ERROR: No path to warehouse found!\n");
        } else {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Warehouse path: " << currentPath.size() << " cells\n");
        }
    }
}

/**
 * @brief Navigate to patient using safety map
 */
void Medic::travelToPatient(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    if (!currentPatient) return;
    
    Position patientPos = currentPatient->getPosition();
    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Calculating DIRECT path to patient at (" 
             << patientPos.x << "," << patientPos.y << ") - prioritizing speed\n");
    
    // Support units prioritize reaching targets quickly - use direct path
    currentPath = AI::findPath(position, patientPos, map);
    
    if (currentPath.empty()) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] ERROR: No path to patient!\n");
    } else {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Patient path: " << currentPath.size() << " cells\n");
    }
    
    pathIndex = 0;
}

/**
 * @brief Heal the current patient
 */
void Medic::healPatient(Map& map) {
    if (!currentPatient || medicineSupplies <= 0) return;
    
    // Heal patient
    Warrior* warrior = dynamic_cast<Warrior*>(currentPatient);
    if (warrior) {
        // Calculate how much healing needed
        int healthNeeded = warrior->getHealthNeeded();
        
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Warrior needs: " << healthNeeded << " HP\n");
        
        // Try to take from warehouse inventory
        int healthToGive = 0;
        if (map.takeMedicine(team, healthNeeded)) {
            healthToGive = healthNeeded;
        } else {
            // Give whatever is available
            int available = map.getMedicineInventory(team);
            if (map.takeMedicine(team, available)) {
                healthToGive = available;
            }
        }
        
        // Heal warrior with what we got
        warrior->heal(healthToGive);
        medicineSupplies--;  // Used one medicine pack
        
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Gave " << healthToGive 
                 << " HP. Warehouse now has: " << map.getMedicineInventory(team) << " medicine\n");
        
        // Clear order
        currentPatient = nullptr;
        currentOrder = Order();
        returningFromWarehouse = false;
    }
}

/**
 * @brief Collect medicine supplies from warehouse
 */
void Medic::collectMedicine(Map& map) {
    // Medic just picks up a "medicine pack" - doesn't take specific amounts yet
    medicineSupplies++;
    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Collected medicine pack (now has " 
             << medicineSupplies << " packs). Warehouse inventory: " 
             << map.getMedicineInventory(team) << " medicine\n");
}
