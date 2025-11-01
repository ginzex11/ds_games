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
    bool retreatSprintActive;       // True only on first turn of retreat (allows multi-tile sprint)
    Position retreatTarget;         // Position to retreat towards (usually medic location)
    Position lastKnownEnemyPosition;
    Position previousPosition;      // Track previous position to detect oscillation
    Position failedDestination;     // Track positions we couldn't reach (to avoid repeated failures)
    int failedAttempts;             // Count failed attempts to reach destination
    int autonomousCooldown;         // Turns remaining in autonomous mode (commander can't override)
    int turnsSinceLastMove;         // Count turns since last actual movement
    int turnsWaitingForMedic;       // Count turns spent waiting for medic
    float lastKnownMedicDistance;   // Track medic distance for progress detection
    int turnsWithoutMedicProgress;  // Count turns medic hasn't gotten closer
    int lastLoggedTurn;  // Track when this warrior last logged to avoid spam
    int lastStandCooldown;          // FIX 1: Prevent immediate re-entry to retreat after Last Stand
    int turnsWithZeroAmmo;          // FIX 2: Track turns spent with 0 ammo for emergency protocol
    int turnsWithoutEnemySighting;  // PRIORITY 2: Track turns since last enemy sighting for active seeking
    Position lastEnemySeenPosition; // PRIORITY 2: Remember last position where enemy was seen
    int turnsSinceLastDamage;       // NEW: Track turns since warrior took damage (for tactical withdrawal)
    int lastDamageTurn;             // NEW: Turn number when warrior last took damage
    
    // Visual effect tracking
    Position lastShotTarget;        // Position of last shot target (for visual effects)
    Position lastGrenadeTarget;     // Position of last grenade throw (for visual effects)
    bool shotThisTurn;              // True if warrior shot this turn
    bool grenadeThisTurn;           // True if warrior threw grenade this turn
    
    void checkResources();
    bool tryShootEnemy(Character* enemy, const Map& map);
    bool tryThrowGrenade(const Position& target, const Map& map, std::vector<Character*>& allCharacters);
    void executeAttackOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeDefendOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeMoveOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void evaluateRetreat(const std::vector<Character*>& allCharacters);  // Check if retreat needed
    void executeRetreat(const Map& map, const std::vector<Character*>& allCharacters);  // Perform retreat
    Position calculateTeamCenter(const std::vector<Character*>& allCharacters) const;  // PRIORITY 2: Calculate team centroid
    bool isTooFarFromTeam(const std::vector<Character*>& allCharacters) const;  // PRIORITY 2: Check team cohesion
    
public:
    Warrior(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    void takeDamage(int damage) override;  // NEW: Override to track damage timing
    
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
    
    // Visual effect queries
    bool didShootThisTurn() const { return shotThisTurn; }
    bool didThrowGrenadeThisTurn() const { return grenadeThisTurn; }
    Position getLastShotTarget() const { return lastShotTarget; }
    Position getLastGrenadeTarget() const { return lastGrenadeTarget; }
    
    // Autonomous mode query (for visualization - warrior acting independently)
    bool isInAutonomousMode() const { return autonomousCooldown > 0; }
    int getAutonomousCooldown() const { return autonomousCooldown; }
    
    // Failed destination query (for commander to check before issuing orders)
    bool hasFailedDestination(const Position& pos) const { 
        return failedDestination == pos && failedAttempts > 0; 
    }
};

#endif // WARRIOR_H
