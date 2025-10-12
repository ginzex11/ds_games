#ifndef COMMANDER_H
#define COMMANDER_H

#include "character.h"

/**
 * @brief Commander class - leads team and coordinates strategy
 * 
 * Responsibilities:
 * - Collect visibility information from entire team
 * - Build comprehensive enemy position map
 * - Issue orders to team members
 * - Relocate to safer positions when threatened
 * - Does not engage in direct combat
 */
class Commander : public Character {
private:
    std::unordered_map<Position, EnemySighting> combinedEnemyMap;
    std::vector<std::vector<float>> teamSafetyMap;
    
    void buildCombinedVisibilityMap(const std::vector<Character*>& teamMembers, int currentTurn);
    void issueOrders(const std::vector<Character*>& teamMembers, const Map& map);
    void relocateIfNeeded(const Map& map);
    Order determineWarriorOrder(Character* warrior, const Map& map);
    Order determineMedicOrder(Character* medic, const std::vector<Character*>& teamMembers, const Map& map);
    Order determineSupplierOrder(Character* supplier, const std::vector<Character*>& teamMembers, const Map& map);
    
public:
    Commander(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    const std::unordered_map<Position, EnemySighting>& getCombinedEnemyMap() const {
        return combinedEnemyMap;
    }
};

#endif // COMMANDER_H
