#include "warrior.h"

/**
 * @brief Construct a new Warrior
 */
Warrior::Warrior(Position pos, Team t)
    : Character(pos, t, CharacterType::WARRIOR),
      ammo(INITIAL_AMMO), grenades(INITIAL_GRENADES),
      needsAmmo(false), needsHealing(false) {
}

/**
 * @brief Update warrior - execute orders or act autonomously
 */
void Warrior::update(const Map& map, const std::vector<Character*>& allCharacters, int currentTurn) {
    if (!alive) return;
    
    // Update visibility and scan
    updateVisibility(map);
    scanForEnemies(allCharacters, currentTurn);
    
    // Check resource status
    checkResources();
    
    // If no order, look for enemies to engage
    if (currentOrder.type == OrderType::NONE && !enemySightings.empty()) {
        // Find nearest enemy
        Character* enemy = findNearestEnemy(allCharacters);
        if (enemy) {
            // Try to shoot
            tryShootEnemy(enemy, map);
        }
    }
    
    // Move along current path
    moveAlongPath();
}

/**
 * @brief Execute order from commander
 */
void Warrior::executeOrder(Order order, const Map& map) {
    currentOrder = order;
    
    // Create mutable copy of allCharacters for grenade throwing
    // This is a limitation - we'll handle it in executeAttackOrder
    
    switch (order.type) {
        case OrderType::MOVE:
            executeMoveOrder(map);
            break;
        case OrderType::ATTACK:
            // Will be handled in update with allCharacters
            lastKnownEnemyPosition = order.targetPosition;
            break;
        case OrderType::DEFEND:
            // Will be handled with proper context
            break;
        default:
            break;
    }
}

/**
 * @brief Check ammo and health levels
 */
void Warrior::checkResources() {
    needsAmmo = (ammo <= LOW_AMMO_THRESHOLD);
    needsHealing = (health <= LOW_HEALTH_THRESHOLD);
}

/**
 * @brief Attempt to shoot at enemy
 */
bool Warrior::tryShootEnemy(Character* enemy, const Map& map) {
    if (!enemy || ammo <= 0) return false;
    
    Position enemyPos = enemy->getPosition();
    float distance = position.euclideanDistance(enemyPos);
    
    // Check if in range and has line of sight
    if (distance <= SHOOT_RANGE && AI::hasLineOfSight(position, enemyPos, map)) {
        ammo--;
        enemy->takeDamage(WARRIOR_DAMAGE);
        return true;
    }
    
    return false;
}

/**
 * @brief Attempt to throw grenade
 */
bool Warrior::tryThrowGrenade(const Position& target, const Map& map, std::vector<Character*>& allCharacters) {
    if (grenades <= 0) return false;
    
    float distance = position.euclideanDistance(target);
    
    // Check if in range
    if (distance <= GRENADE_RANGE) {
        grenades--;
        
        // Apply area damage
        for (Character* c : allCharacters) {
            if (!c->isAlive()) continue;
            
            float distToTarget = c->getPosition().euclideanDistance(target);
            if (distToTarget <= GRENADE_RADIUS) {
                // Damage decreases with distance from epicenter
                int damage = static_cast<int>(GRENADE_DAMAGE * (1.0f - distToTarget / GRENADE_RADIUS));
                c->takeDamage(damage);
            }
        }
        return true;
    }
    
    return false;
}

/**
 * @brief Execute attack order
 */
void Warrior::executeAttackOrder(const Map& map, const std::vector<Character*>& allCharacters) {
    if (currentOrder.type != OrderType::ATTACK) return;
    
    Position target = currentOrder.targetPosition;
    
    // Find path to attack position
    Position attackPos = AI::findAttackPosition(position, target, SHOOT_RANGE, map);
    
    if (attackPos != position && currentPath.empty()) {
        // Generate safety map
        std::vector<Position> enemyPos;
        for (Character* c : allCharacters) {
            if (c->isAlive() && c->getTeam() != team) {
                enemyPos.push_back(c->getPosition());
            }
        }
        auto safetyMap = AI::generateSafetyMap(enemyPos, map);
        
        currentPath = AI::findPath(position, attackPos, map, &safetyMap, 0.3f);
        pathIndex = 0;
    }
}

/**
 * @brief Execute defend order
 */
void Warrior::executeDefendOrder(const Map& map, const std::vector<Character*>& allCharacters) {
    if (currentOrder.type != OrderType::DEFEND) return;
    
    // Generate safety map
    std::vector<Position> enemyPos;
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() != team) {
            enemyPos.push_back(c->getPosition());
        }
    }
    auto safetyMap = AI::generateSafetyMap(enemyPos, map);
    
    // Find safe position
    Position safePos = AI::findNearestSafePosition(position, map, safetyMap, 2.0f);
    
    if (safePos != position && currentPath.empty()) {
        currentPath = AI::findPath(position, safePos, map, &safetyMap, 1.0f);
        pathIndex = 0;
    }
    
    // Still try to shoot if enemy visible
    Character* enemy = findNearestEnemy(allCharacters);
    if (enemy) {
        tryShootEnemy(enemy, map);
    }
}

/**
 * @brief Execute move order
 */
void Warrior::executeMoveOrder(const Map& map) {
    if (currentPath.empty()) {
        currentPath = AI::findPath(position, currentOrder.targetPosition, map);
        pathIndex = 0;
    }
}
