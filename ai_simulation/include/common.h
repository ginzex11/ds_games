#ifndef COMMON_H
#define COMMON_H

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "Logger.h"

// Logging macros that support stream-style syntax
#define LOG_CHARACTER(msg) do { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::log(Logger::LogType::CHARACTER, oss.str()); \
} while(0)

#define LOG_CONTROL(msg) do { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::log(Logger::LogType::CONTROL, oss.str()); \
} while(0)

// Forward declarations
class Character;
class Commander;
class Warrior;
class Medic;
class Supplier;

// Constants
constexpr int GRID_WIDTH = 40;
constexpr int GRID_HEIGHT = 30;
constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 900;
constexpr float CELL_SIZE = 30.0f;

// Game balance constants
constexpr int INITIAL_HEALTH = 100;
constexpr int INITIAL_AMMO = 15;           // Reduced from 50 - enough for 1.5 kills, forces resupply
constexpr int INITIAL_GRENADES = 2;        // Reduced from 3 - grenades more precious
constexpr int WARRIOR_DAMAGE = 10;         // 10 shots to kill
constexpr int GRENADE_DAMAGE = 40;
constexpr int GRENADE_RADIUS = 2;
constexpr int SHOOT_RANGE = 8;
constexpr int GRENADE_RANGE = 6;
constexpr int VISIBILITY_RANGE = 10;
constexpr int LOW_HEALTH_THRESHOLD = 50;       // Request healing at 50% health
constexpr int RETREAT_HEALTH_THRESHOLD = 40;   // Start retreating at 40% health (with time to escape)
constexpr int CRITICAL_HEALTH_THRESHOLD = 25;  // Critical danger zone at 25% health
constexpr int LOW_AMMO_THRESHOLD = 8;          // Reduced from 25 - request ammo at ~50% (8/15)
constexpr int WAREHOUSE_RESUPPLY_AMOUNT = 12;  // Reduced from 20 - gives 12 bullets
constexpr int MEDICINE_HEAL_AMOUNT = 100;

// Enumerations
enum class Team {
    BLUE,
    ORANGE
};

enum class CharacterType {
    COMMANDER,
    WARRIOR,
    MEDIC,
    SUPPLIER
};

enum class CellType {
    EMPTY,
    ROCK,      // Impassable, blocks sight and shots
    TREE,      // Passable, blocks sight and shots
    WATER,     // Impassable, allows sight and shots
    WAREHOUSE  // Special cells for resupply
};

enum class WarehouseType {
    NONE,
    AMMO,
    MEDICINE
};

enum class OrderType {
    NONE,
    MOVE,
    ATTACK,
    DEFEND,
    HEAL,
    RESUPPLY
};

// Position structure
struct Position {
    int x;
    int y;
    
    Position() : x(0), y(0) {}
    Position(int x_, int y_) : x(x_), y(y_) {}
    
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
    
    // Manhattan distance
    int manhattanDistance(const Position& other) const {
        return std::abs(x - other.x) + std::abs(y - other.y);
    }
    
    // Euclidean distance
    float euclideanDistance(const Position& other) const {
        int dx = x - other.x;
        int dy = y - other.y;
        return std::sqrt(static_cast<float>(dx * dx + dy * dy));
    }
};

// Hash function for Position to use in unordered_map/set
namespace std {
    template <>
    struct hash<Position> {
        size_t operator()(const Position& pos) const {
            return hash<int>()(pos.x) ^ (hash<int>()(pos.y) << 1);
        }
    };
}

// Order structure for commander to give
struct Order {
    OrderType type;
    Position targetPosition;
    Character* targetCharacter;
    
    Order() : type(OrderType::NONE), targetPosition(0, 0), targetCharacter(nullptr) {}
    Order(OrderType t, Position pos = Position(0, 0), Character* target = nullptr)
        : type(t), targetPosition(pos), targetCharacter(target) {}
};

// Cell information structure
struct Cell {
    CellType type;
    WarehouseType warehouseType;
    
    Cell() : type(CellType::EMPTY), warehouseType(WarehouseType::NONE) {}
    Cell(CellType t, WarehouseType wt = WarehouseType::NONE)
        : type(t), warehouseType(wt) {}
    
    bool isPassable() const {
        return type == CellType::EMPTY || type == CellType::TREE || type == CellType::WAREHOUSE;
    }
    
    bool blocksSight() const {
        return type == CellType::ROCK || type == CellType::TREE;
    }
};

// Enemy sighting report
struct EnemySighting {
    Position position;
    CharacterType type;
    int turnSeen;
    
    EnemySighting() : position(0, 0), type(CharacterType::WARRIOR), turnSeen(0) {}
    EnemySighting(Position pos, CharacterType t, int turn)
        : position(pos), type(t), turnSeen(turn) {}
};

// Utility functions
inline bool isValidPosition(const Position& pos) {
    return pos.x >= 0 && pos.x < GRID_WIDTH && pos.y >= 0 && pos.y < GRID_HEIGHT;
}

inline std::string teamToString(Team team) {
    return (team == Team::BLUE) ? "Blue" : "Orange";
}

inline std::string characterTypeToString(CharacterType type) {
    switch (type) {
        case CharacterType::COMMANDER: return "Commander";
        case CharacterType::WARRIOR: return "Warrior";
        case CharacterType::MEDIC: return "Medic";
        case CharacterType::SUPPLIER: return "Supplier";
        default: return "Unknown";
    }
}

inline char characterTypeToChar(CharacterType type) {
    switch (type) {
        case CharacterType::COMMANDER: return 'C';
        case CharacterType::WARRIOR: return 'W';
        case CharacterType::MEDIC: return 'M';
        case CharacterType::SUPPLIER: return 'P';
        default: return '?';
    }
}

inline std::string orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType::NONE: return "NONE";
        case OrderType::MOVE: return "MOVE";
        case OrderType::ATTACK: return "ATTACK";
        case OrderType::DEFEND: return "DEFEND";
        case OrderType::HEAL: return "HEAL";
        case OrderType::RESUPPLY: return "RESUPPLY";
        default: return "Unknown";
    }
}

#endif // COMMON_H
