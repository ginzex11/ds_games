#include "supplier.h"
#include "warrior.h"

/**
 * @brief Construct a new Supplier
 */
Supplier::Supplier(Position pos, Team t)
    : Character(pos, t, CharacterType::SUPPLIER),
      ammoSupplies(0), currentRecipient(nullptr), returningFromWarehouse(false) {
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
        if (!currentRecipient->isAlive()) {
            // Recipient died, clear order
            currentRecipient = nullptr;
            currentOrder = Order();
            returningFromWarehouse = false;
        } else if (position == currentRecipient->getPosition()) {
            // Reached recipient, resupply them
            resupplyRecipient();
        } else if (!hasAmmo() && !returningFromWarehouse) {
            // Need to get ammo first
            travelToWarehouse(map);
        } else if (returningFromWarehouse && hasAmmo()) {
            // Have ammo, now go to recipient
            travelToRecipient(map);
        }
    }
    
    // Move along path
    moveAlongPath();
    
    // Check if reached warehouse
    if (map.isWarehouse(position) && 
        map.getWarehouseType(position) == WarehouseType::AMMO) {
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
