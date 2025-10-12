#ifndef WARRIOR_H
#define WARRIOR_H

#include "character.h"

/**
 * @brief Warrior class - combat specialist
 * 
 * Capabilities:
 * - Shoot at enemies (requires ammo and line of sight)
 * - Throw grenades for area damage
 * - Follow attack, move, and defend orders
 * - Request resupply when ammo is low
 * - Request medical attention when health is low
 */
class Warrior : public Character {
private:
    int ammo;
    int grenades;
    bool needsAmmo;
    bool needsHealing;
    bool isRetreating;              // True when warrior is actively retreating to safety
    Position retreatTarget;         // Position to retreat towards (usually medic location)
    Position lastKnownEnemyPosition;
    Position previousPosition;      // Track previous position to detect oscillation
    Position failedDestination;     // Track positions we couldn't reach (to avoid repeated failures)
    int failedAttempts;             // Count failed attempts to reach destination
    int turnsSinceLastMove;         // Count turns since last actual movement
    int lastLoggedTurn;  // Track when this warrior last logged to avoid spam
    
    void checkResources();
    bool tryShootEnemy(Character* enemy, const Map& map);
    bool tryThrowGrenade(const Position& target, const Map& map, std::vector<Character*>& allCharacters);
    void executeAttackOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeDefendOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeMoveOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void evaluateRetreat(const std::vector<Character*>& allCharacters);  // Check if retreat needed
    void executeRetreat(const Map& map, const std::vector<Character*>& allCharacters);  // Perform retreat
    
public:
    Warrior(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    // Resource management
    void resupplyAmmo(int ammoAmount, int grenadeAmount);  // Now takes specific amounts and prevents overflow
    void heal(int amount);  // Updated to prevent overflow
    
    // Calculate how much warrior needs
    int getAmmoNeeded() const { return INITIAL_AMMO - ammo; }
    int getGrenadesNeeded() const { return INITIAL_GRENADES - grenades; }
    int getHealthNeeded() const { return INITIAL_HEALTH - health; }
    
    // Status queries
    bool getNeedsAmmo() const { return needsAmmo; }
    bool getNeedsHealing() const { return needsHealing; }
    bool getIsRetreating() const { return isRetreating; }
    int getAmmo() const { return ammo; }
    int getGrenades() const { return grenades; }
};

#endif // WARRIOR_H
