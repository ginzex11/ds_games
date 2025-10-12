#ifndef MAP_H
#define MAP_H

#include "common.h"

/**
 * @brief Map class represents the game grid with obstacles and warehouses
 * 
 * Manages the grid-based map including:
 * - Terrain types (rocks, trees, water)
 * - Warehouse locations for ammo and medicine
 * - Cell queries for pathfinding and visibility
 */
class Map {
private:
    std::vector<std::vector<Cell>> grid;
    Position blueAmmoWarehouse;
    Position blueMedicineWarehouse;
    Position orangeAmmoWarehouse;
    Position orangeMedicineWarehouse;
    
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
    
    // Utility
    void reset();
    int getWidth() const { return GRID_WIDTH; }
    int getHeight() const { return GRID_HEIGHT; }
};

#endif // MAP_H
