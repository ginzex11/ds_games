#ifndef MEDIC_H
#define MEDIC_H

#include "character.h"

/**
 * @brief Medic class - team healer
 * 
 * Responsibilities:
 * - Travel to medicine warehouse to get supplies
 * - Move to injured warriors to heal them
 * - Follow commander's orders for healing priorities
 * - Non-combatant (doesn't fight)
 */
class Medic : public Character {
private:
    int medicineSupplies;
    Character* currentPatient;
    bool returningFromWarehouse;
    
    void travelToWarehouse(const Map& map, const std::vector<std::vector<float>>& safetyMap);
    void travelToPatient(const Map& map, const std::vector<std::vector<float>>& safetyMap);
    void healPatient();
    
public:
    Medic(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    bool hasMedicine() const { return medicineSupplies > 0; }
    void collectMedicine() { medicineSupplies++; }
    int getMedicineSupplies() const { return medicineSupplies; }  // Get current medicine count for UI
    Character* getCurrentPatient() const { return currentPatient; }  // For commander to check assignment
};

#endif // MEDIC_H
