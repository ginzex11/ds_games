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
        } else {
            // Check if close enough to patient (within 2 cells - manhattan distance)
            float distance = position.manhattanDistance(currentPatient->getPosition());
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Distance to patient: " << distance 
                     << " | Has medicine: " << (hasMedicine() ? "Yes" : "No") 
                     << " | Supplies: " << medicineSupplies << "\n");
            
            if (distance <= 2) {
                // Close enough - can heal (allows diagonal adjacency)
                if (hasMedicine()) {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Healing patient!\n");
                    healPatient();
                } else {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] No medicine, going to warehouse\n");
                    travelToWarehouse(map, safetyMap);
                }
            } else if (!hasMedicine() && !returningFromWarehouse) {
                // Need to get medicine first
                LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Going to warehouse for supplies\n");
                travelToWarehouse(map, safetyMap);
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
    
    // Check if reached warehouse
    if (map.isWarehouse(position) && 
        map.getWarehouseType(position) == WarehouseType::MEDICINE) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Reached warehouse, collecting medicine\n");
        collectMedicine();
        returningFromWarehouse = true;
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
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Calculating path to warehouse at (" 
                 << warehouse.x << "," << warehouse.y << ") using safety map\n");
        // Use lower safety weight for support units to ensure they can reach warehouses
        currentPath = AI::findPath(position, warehouse, map, &safetyMap, 0.1f);
        
        // Fallback: If no safe path, try direct path (medics MUST reach warehouse!)
        if (currentPath.empty()) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] No safe path, trying direct route\n");
            currentPath = AI::findPath(position, warehouse, map);
        }
        
        pathIndex = 0;
        
        if (currentPath.empty()) {
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] WARNING: No path to warehouse found!\n");
        }
    }
}

/**
 * @brief Navigate to patient using safety map
 */
void Medic::travelToPatient(const Map& map, const std::vector<std::vector<float>>& safetyMap) {
    if (!currentPatient) return;
    
    Position patientPos = currentPatient->getPosition();
    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Calculating path to patient at (" 
             << patientPos.x << "," << patientPos.y << ") using safety map\n");
    // Use lower safety weight for support units to ensure they can reach targets
    currentPath = AI::findPath(position, patientPos, map, &safetyMap, 0.1f);
    
    // Fallback: If no safe path, try direct path (medics must reach patients!)
    if (currentPath.empty()) {
        LOG_CHARACTER("[MEDIC " << teamToString(team) << "] No safe path, trying direct route\n");
        currentPath = AI::findPath(position, patientPos, map);
    }
    
    pathIndex = 0;
}

/**
 * @brief Heal the current patient
 */
void Medic::healPatient() {
    if (!currentPatient || medicineSupplies <= 0) return;
    
    // Heal patient to full health
    Warrior* warrior = dynamic_cast<Warrior*>(currentPatient);
    if (warrior) {
        warrior->heal(MEDICINE_HEAL_AMOUNT);
        medicineSupplies--;
        
        // Clear order
        currentPatient = nullptr;
        currentOrder = Order();
        returningFromWarehouse = false;
    }
}
