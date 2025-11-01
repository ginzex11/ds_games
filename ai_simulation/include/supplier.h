#ifndef SUPPLIER_H
#define SUPPLIER_H

#include "character.h"

/**
 * @brief Supplier class - provides ammunition
 * 
 * Responsibilities:
 * - Travel to ammo warehouse to get supplies
 * - Move to warriors needing ammo to resupply them
 * - Follow commander's orders for resupply priorities
 * - Non-combatant (doesn't fight)
 */
class Supplier : public Character {
private:
    int ammoSupplies;
    Character* currentRecipient;
    bool returningFromWarehouse;
    
    // Recipient pathfinding failure tracking
    int recipientPathFailures;
    int lastRecipientPathAttempt;
    int currentTurnTracker;
    
    void travelToWarehouse(const Map& map, const std::vector<std::vector<float>>& safetyMap);
    void travelToRecipient(const Map& map, const std::vector<std::vector<float>>& safetyMap);
    void resupplyRecipient(Map& map);  // Now takes map to check warrior needs
    
public:
    Supplier(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    bool hasAmmo() const { return ammoSupplies > 0; }
    void collectAmmo(Map& map);  // Now takes from warehouse inventory
    int getAmmoSupplies() const { return ammoSupplies; }  // Get current ammo supply count for UI
};

#endif // SUPPLIER_H
