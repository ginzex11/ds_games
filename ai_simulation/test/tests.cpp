/**
 * @file tests.cpp
 * @brief Basic unit tests for AI Simulation Game
 * 
 * Tests core functionality:
 * - Map generation and queries
 * - A* pathfinding
 * - Line of sight calculations
 * - Character behavior
 */

#include "../include/common.h"
#include "../include/map.h"
#include "../include/ai.h"
#include "../include/character.h"
#include "../include/commander.h"
#include "../include/warrior.h"
#include "../include/medic.h"
#include "../include/supplier.h"
#include <iostream>
#include <cassert>

// Test counter
int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) \
    std::cout << "Running test: " << name << "... "; \
    try {

#define END_TEST \
        testsPassed++; \
        std::cout << "PASSED\n"; \
    } catch (const std::exception& e) { \
        testsFailed++; \
        std::cout << "FAILED: " << e.what() << "\n"; \
    }

/**
 * @brief Test map initialization and queries
 */
void testMap() {
    TEST("Map Initialization")
        Map map;
        assert(map.getWidth() == GRID_WIDTH);
        assert(map.getHeight() == GRID_HEIGHT);
    END_TEST
    
    TEST("Map Passability")
        Map map;
        // Most cells should be passable
        int passableCount = 0;
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (map.isPassable(Position(x, y))) {
                    passableCount++;
                }
            }
        }
        assert(passableCount > GRID_WIDTH * GRID_HEIGHT / 2);
    END_TEST
    
    TEST("Warehouse Locations")
        Map map;
        Position blueAmmo = map.getWarehouse(Team::BLUE, WarehouseType::AMMO);
        Position blueMed = map.getWarehouse(Team::BLUE, WarehouseType::MEDICINE);
        Position orangeAmmo = map.getWarehouse(Team::ORANGE, WarehouseType::AMMO);
        Position orangeMed = map.getWarehouse(Team::ORANGE, WarehouseType::MEDICINE);
        
        assert(isValidPosition(blueAmmo));
        assert(isValidPosition(blueMed));
        assert(isValidPosition(orangeAmmo));
        assert(isValidPosition(orangeMed));
        
        assert(map.isWarehouse(blueAmmo));
        assert(map.getWarehouseType(blueAmmo) == WarehouseType::AMMO);
    END_TEST
}

/**
 * @brief Test A* pathfinding
 */
void testPathfinding() {
    TEST("A* Basic Pathfinding")
        Map map;
        Position start(5, 5);
        Position goal(10, 10);
        
        std::vector<Position> path = AI::findPath(start, goal, map);
        
        // Path should exist and not be empty
        assert(!path.empty());
        // First position should be start (or very close)
        assert(path.front().manhattanDistance(start) <= 1);
        // Last position should be goal
        assert(path.back() == goal);
    END_TEST
    
    TEST("A* Path to Same Position")
        Map map;
        Position pos(5, 5);
        std::vector<Position> path = AI::findPath(pos, pos, map);
        assert(!path.empty());
        assert(path.front() == pos);
    END_TEST
    
    TEST("A* Path Validity")
        Map map;
        Position start(5, 5);
        Position goal(15, 10);
        std::vector<Position> path = AI::findPath(start, goal, map);
        
        // All positions in path should be valid and passable
        for (const Position& pos : path) {
            assert(isValidPosition(pos));
            assert(map.isPassable(pos));
        }
    END_TEST
}

/**
 * @brief Test line of sight calculations
 */
void testLineOfSight() {
    TEST("Line of Sight - Clear Path")
        Map map;
        // Test line of sight between adjacent positions (guaranteed clear)
        Position pos1(5, 5);
        Position pos2(6, 5);
        
        // Adjacent positions should always have line of sight
        bool hasLOS = AI::hasLineOfSight(pos1, pos2, map);
        assert(hasLOS);
    END_TEST
    
    TEST("Visibility Calculation")
        Map map;
        Position center(10, 10);
        auto visible = AI::calculateVisibility(center, 5, map);
        
        // Should see at least some cells
        assert(!visible.empty());
        // Center should be visible to itself
        assert(visible.count(center) > 0);
    END_TEST
}

/**
 * @brief Test character creation and basic properties
 */
void testCharacters() {
    TEST("Character Creation - Commander")
        Commander commander(Position(5, 5), Team::BLUE);
        assert(commander.getType() == CharacterType::COMMANDER);
        assert(commander.getTeam() == Team::BLUE);
        assert(commander.isAlive());
        assert(commander.getHealth() == INITIAL_HEALTH);
    END_TEST
    
    TEST("Character Creation - Warrior")
        Warrior warrior(Position(10, 10), Team::ORANGE);
        assert(warrior.getType() == CharacterType::WARRIOR);
        assert(warrior.getTeam() == Team::ORANGE);
        assert(warrior.getAmmo() == INITIAL_AMMO);
        assert(warrior.getGrenades() == INITIAL_GRENADES);
    END_TEST
    
    TEST("Character Damage")
        Warrior warrior(Position(10, 10), Team::BLUE);
        int initialHealth = warrior.getHealth();
        warrior.takeDamage(30);
        assert(warrior.getHealth() == initialHealth - 30);
        assert(warrior.isAlive());
        
        warrior.takeDamage(1000);
        assert(warrior.getHealth() == 0);
        assert(!warrior.isAlive());
    END_TEST
    
    TEST("Character Movement")
        Commander commander(Position(5, 5), Team::BLUE);
        Position newPos(6, 6);
        commander.setPosition(newPos);
        assert(commander.getPosition() == newPos);
    END_TEST
}

/**
 * @brief Test safety map generation
 */
void testSafetyMap() {
    TEST("Safety Map Generation")
        Map map;
        std::vector<Position> enemies = {
            Position(10, 10),
            Position(20, 20)
        };
        
        auto safetyMap = AI::generateSafetyMap(enemies, map);
        
        assert(safetyMap.size() == GRID_HEIGHT);
        assert(safetyMap[0].size() == GRID_WIDTH);
        
        // Positions near enemies should have higher threat
        float threatNearEnemy = safetyMap[10][10];
        float threatFarAway = safetyMap[0][0];
        assert(threatNearEnemy >= threatFarAway);
    END_TEST
}

/**
 * @brief Test Position structure utilities
 */
void testPositionUtilities() {
    TEST("Position Manhattan Distance")
        Position p1(0, 0);
        Position p2(3, 4);
        assert(p1.manhattanDistance(p2) == 7);
    END_TEST
    
    TEST("Position Euclidean Distance")
        Position p1(0, 0);
        Position p2(3, 4);
        float dist = p1.euclideanDistance(p2);
        assert(std::abs(dist - 5.0f) < 0.01f);
    END_TEST
    
    TEST("Position Equality")
        Position p1(5, 10);
        Position p2(5, 10);
        Position p3(5, 11);
        assert(p1 == p2);
        assert(p1 != p3);
    END_TEST
}

/**
 * @brief Test character type utilities
 */
void testUtilities() {
    TEST("Character Type to String")
        std::string commander = characterTypeToString(CharacterType::COMMANDER);
        assert(commander == "Commander");
        
        std::string warrior = characterTypeToString(CharacterType::WARRIOR);
        assert(warrior == "Warrior");
    END_TEST
    
    TEST("Character Type to Char")
        assert(characterTypeToChar(CharacterType::COMMANDER) == 'C');
        assert(characterTypeToChar(CharacterType::WARRIOR) == 'W');
        assert(characterTypeToChar(CharacterType::MEDIC) == 'M');
        assert(characterTypeToChar(CharacterType::SUPPLIER) == 'P');
    END_TEST
    
    TEST("Team to String")
        assert(teamToString(Team::BLUE) == "Blue");
        assert(teamToString(Team::ORANGE) == "Orange");
    END_TEST
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "=================================================\n";
    std::cout << "  AI Simulation Game - Unit Tests\n";
    std::cout << "=================================================\n\n";
    
    std::cout << "Running Map Tests...\n";
    testMap();
    
    std::cout << "\nRunning Pathfinding Tests...\n";
    testPathfinding();
    
    std::cout << "\nRunning Line of Sight Tests...\n";
    testLineOfSight();
    
    std::cout << "\nRunning Character Tests...\n";
    testCharacters();
    
    std::cout << "\nRunning Safety Map Tests...\n";
    testSafetyMap();
    
    std::cout << "\nRunning Position Utility Tests...\n";
    testPositionUtilities();
    
    std::cout << "\nRunning Utility Tests...\n";
    testUtilities();
    
    std::cout << "\n=================================================\n";
    std::cout << "Test Results:\n";
    std::cout << "  Passed: " << testsPassed << "\n";
    std::cout << "  Failed: " << testsFailed << "\n";
    std::cout << "  Total:  " << (testsPassed + testsFailed) << "\n";
    std::cout << "=================================================\n";
    
    if (testsFailed == 0) {
        std::cout << "\n All tests passed!\n\n";
        return 0;
    } else {
        std::cout << "\n Some tests failed.\n\n";
        return 1;
    }
}
