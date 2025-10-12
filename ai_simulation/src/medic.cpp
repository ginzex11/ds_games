#include "medic.h"
#include "warrior.h"

/**
 * @brief Construct a new Medic
 */
Medic::Medic(Position pos, Team t)
    : Character(pos, t, CharacterType::MEDIC),
      medicineSupplies(3), currentPatient(nullptr), returningFromWarehouse(false) {
}

/**
 * @brief Update medic - heal warriors or get supplies
 */
void Medic::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Update visibility
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
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
            // Patient died, clear order
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Patient died, clearing order\n");
            currentPatient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
        } else {
            // Check if adjacent to patient (within 1 cell)
            float distance = position.manhattanDistance(currentPatient->getPosition());
            LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Distance to patient: " << distance 
                     << " | Has medicine: " << (hasMedicine() ? "Yes" : "No") 
                     << " | Supplies: " << medicineSupplies << "\n");
            
            if (distance <= 1) {
                // Adjacent or same cell - can heal
                if (hasMedicine()) {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Healing patient!\n");
                    healPatient();
                } else {
                    LOG_CHARACTER("[MEDIC " << teamToString(team) << "] No medicine, going to warehouse\n");
                    travelToWarehouse(map);
                }
            } else if (!hasMedicine() && !returningFromWarehouse) {
                // Need to get medicine first
                LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Going to warehouse for supplies\n");
                travelToWarehouse(map);
            } else {
                // Have medicine or returning, move towards patient
                LOG_CHARACTER("[MEDIC " << teamToString(team) << "] Moving towards patient at (" 
                         << currentPatient->getPosition().x << "," << currentPatient->getPosition().y << ")"
                         << " | Path size: " << currentPath.size() << " | PathIndex: " << pathIndex << "\n");
                travelToPatient(map);
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
        
        if (!hasMedicine()) {
            travelToWarehouse(map);
        } else {
            travelToPatient(map);
        }
    } else if (order.type == OrderType::MOVE) {
        currentPath = AI::findPath(position, order.targetPosition, map);
        pathIndex = 0;
    }
}

/**
 * @brief Navigate to medicine warehouse
 */
void Medic::travelToWarehouse(const Map& map) {
    Position warehouse = map.getWarehouse(team, WarehouseType::MEDICINE);
    currentPath = AI::findPath(position, warehouse, map);
    pathIndex = 0;
}

/**
 * @brief Navigate to patient
 */
void Medic::travelToPatient(const Map& map) {
    if (!currentPatient) return;
    
    Position patientPos = currentPatient->getPosition();
    currentPath = AI::findPath(position, patientPos, map);
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
