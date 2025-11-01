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
    std::unordered_set<Position> teamVisibilityMap;  // Combined visibility from all warriors
    
    void buildCombinedVisibilityMap(const std::vector<Character*>& teamMembers, int currentTurn);
    void aggregateTeamVisibility(const std::vector<Character*>& teamMembers);
    void issueOrders(const std::vector<Character*>& teamMembers, const Map& map, int currentTurn);
    void relocateIfNeeded(const Map& map);
    Order determineWarriorOrder(Character* warrior, const Map& map, int currentTurn);
    Order determineMedicOrder(Character* medic, const std::vector<Character*>& teamMembers, const Map& map);
    Order determineSupplierOrder(Character* supplier, const std::vector<Character*>& teamMembers, const Map& map);
    
public:
    Commander(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    const std::unordered_map<Position, EnemySighting>& getCombinedEnemyMap() const {
        return combinedEnemyMap;
    }
    
    const std::unordered_set<Position>& getTeamVisibilityMap() const {
        return teamVisibilityMap;
    }
    
    // Check if team can see a position (commander or any team member)
    bool teamCanSee(const Position& pos) const {
        return teamVisibilityMap.count(pos) > 0;
    }
};

#endif // COMMANDER_H
