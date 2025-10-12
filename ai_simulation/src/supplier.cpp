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
    
    // Update visibility
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
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
            // Recipient died, clear order
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Recipient died, clearing order\n");
            currentRecipient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
        } else {
            // Check if adjacent to recipient (within 1 cell)
            float distance = position.manhattanDistance(currentRecipient->getPosition());
            LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Distance to recipient: " << distance 
                     << " | Has ammo: " << (hasAmmo() ? "Yes" : "No")
                     << " | Supplies: " << ammoSupplies << "\n");
            
            if (distance <= 1) {
                // Adjacent or same cell - can resupply
                if (hasAmmo()) {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Resupplying recipient!\n");
                    resupplyRecipient();
                } else {
                    LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] No ammo, going to warehouse\n");
                    travelToWarehouse(map);
                }
            } else if (!hasAmmo() && !returningFromWarehouse) {
                // Need to get ammo first
                LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Going to warehouse for supplies\n");
                travelToWarehouse(map);
            } else {
                // Have ammo or returning, move towards recipient
                LOG_CHARACTER("[SUPPLIER " << teamToString(team) << "] Moving towards recipient at (" 
                         << currentRecipient->getPosition().x << "," << currentRecipient->getPosition().y << ")\n");
                travelToRecipient(map);
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
        
        if (!hasAmmo()) {
            travelToWarehouse(map);
        } else {
            travelToRecipient(map);
        }
    } else if (order.type == OrderType::MOVE) {
        currentPath = AI::findPath(position, order.targetPosition, map);
        pathIndex = 0;
    }
}

/**
 * @brief Navigate to ammo warehouse
 */
void Supplier::travelToWarehouse(const Map& map) {
    Position warehouse = map.getWarehouse(team, WarehouseType::AMMO);
    currentPath = AI::findPath(position, warehouse, map);
    pathIndex = 0;
}

/**
 * @brief Navigate to recipient
 */
void Supplier::travelToRecipient(const Map& map) {
    if (!currentRecipient) return;
    
    Position recipientPos = currentRecipient->getPosition();
    currentPath = AI::findPath(position, recipientPos, map);
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
