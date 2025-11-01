#ifndef MAP_H
#define MAP_H

#include "common.h"

/**
 * @brief Warehouse inventory tracking
 */
struct WarehouseInventory {
    int ammo;        // Bullets available
    int grenades;    // Grenades available
    int medicine;    // Medicine supplies available
    
    WarehouseInventory() : ammo(0), grenades(0), medicine(0) {}
};

/**
 * @brief Map class represents the game grid with obstacles and warehouses
 * 
 * Manages the grid-based map including:
 * - Terrain types (rocks, trees, water)
 * - Warehouse locations for ammo and medicine
 * - Warehouse inventory (bullets, grenades, medicine)
 * - Cell queries for pathfinding and visibility
 */
class Map {
private:
    std::vector<std::vector<Cell>> grid;
    Position blueAmmoWarehouse;
    Position blueMedicineWarehouse;
    Position orangeAmmoWarehouse;
    Position orangeMedicineWarehouse;
    
    // Warehouse inventories
    WarehouseInventory blueAmmoInventory;
    WarehouseInventory blueMedicineInventory;
    WarehouseInventory orangeAmmoInventory;
    WarehouseInventory orangeMedicineInventory;
    
    void generateObstacles();
    void placeWarehouses();
    void createObstacleCluster(Position center, CellType type, int radius);

public:
    Map();
    
    // Grid access
    const Cell& getCell(const Position& pos) const;
    const Cell& getCell(int x, int y) const;
    bool isPassable(const Position& pos) const;
    bool blocksSight(const Position& pos) const;
    
    // Warehouse queries
    Position getWarehouse(Team team, WarehouseType type) const;
    bool isWarehouse(const Position& pos) const;
    WarehouseType getWarehouseType(const Position& pos) const;
    Team getWarehouseTeam(const Position& pos) const;  // Get which team owns this warehouse
    
    // Warehouse inventory management
    bool takeAmmo(Team team, int amount);          // Try to take ammo, returns true if available
    bool takeGrenades(Team team, int amount);      // Try to take grenades, returns true if available
    bool takeMedicine(Team team, int amount);      // Try to take medicine, returns true if available
    int getAmmoInventory(Team team) const;         // Get current ammo count
    int getGrenadeInventory(Team team) const;      // Get current grenade count
    int getMedicineInventory(Team team) const;     // Get current medicine count
    
    // Utility
    void reset();
    int getWidth() const { return GRID_WIDTH; }
    int getHeight() const { return GRID_HEIGHT; }
    void logMapLayout() const;  // Log the entire map layout to map_layout.txt
};

#endif // MAP_H
