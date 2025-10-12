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
    
    void travelToWarehouse(const Map& map);
    void travelToRecipient(const Map& map);
    void resupplyRecipient();
    
public:
    Supplier(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    bool hasAmmo() const { return ammoSupplies > 0; }
    void collectAmmo() { ammoSupplies++; }
};

#endif // SUPPLIER_H
