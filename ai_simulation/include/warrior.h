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
    Position lastKnownEnemyPosition;
    
    void checkResources();
    bool tryShootEnemy(Character* enemy, const Map& map);
    bool tryThrowGrenade(const Position& target, const Map& map, std::vector<Character*>& allCharacters);
    void executeAttackOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeDefendOrder(const Map& map, const std::vector<Character*>& allCharacters);
    void executeMoveOrder(const Map& map);
    
public:
    Warrior(Position pos, Team t);
    
    void update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) override;
    void executeOrder(Order order, const Map& map) override;
    
    // Resource management
    void resupplyAmmo(int amount) { ammo += amount; needsAmmo = false; }
    void heal(int amount) { health += amount; if (health > INITIAL_HEALTH) health = INITIAL_HEALTH; needsHealing = false; }
    
    // Status queries
    bool getNeedsAmmo() const { return needsAmmo; }
    bool getNeedsHealing() const { return needsHealing; }
    int getAmmo() const { return ammo; }
    int getGrenades() const { return grenades; }
};

#endif // WARRIOR_H
