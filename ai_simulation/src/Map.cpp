#include "map.h"
#include <random>
#include <ctime>

/**
 * @brief Construct a new Map object and generate terrain
 */
Map::Map() {
    // Initialize grid with empty cells
    grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
    
    // Generate map features
    generateObstacles();
    placeWarehouses();
}

/**
 * @brief Generate scattered obstacles (rocks, trees, water) on the map
 * Changed from clusters to scattered distribution for better gameplay
 */
void Map::generateObstacles() {
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_int_distribution<int> xDist(3, GRID_WIDTH - 4);
    std::uniform_int_distribution<int> yDist(3, GRID_HEIGHT - 4);
    std::uniform_int_distribution<int> typeDist(0, 2);
    
    // Calculate total obstacles to place (about 15-20% of map)
    int totalCells = GRID_WIDTH * GRID_HEIGHT;
    int targetObstacles = static_cast<int>(totalCells * 0.17f);  // 17% coverage
    
    // Place individual obstacles scattered across the map
    int placed = 0;
    int attempts = 0;
    int maxAttempts = targetObstacles * 3;  // Prevent infinite loops
    
    while (placed < targetObstacles && attempts < maxAttempts) {
        attempts++;
        
        Position pos(xDist(rng), yDist(rng));
        
        // Skip if already occupied
        if (grid[pos.y][pos.x].type != CellType::EMPTY) {
            continue;
        }
        
        // Determine obstacle type
        CellType type;
        int typeChoice = typeDist(rng);
        if (typeChoice == 0) type = CellType::ROCK;
        else if (typeChoice == 1) type = CellType::TREE;
        else type = CellType::WATER;
        
        // Place the obstacle
        grid[pos.y][pos.x] = Cell(type);
        placed++;
        
        // Small chance to place 1-2 adjacent obstacles for variety (not full clusters)
        if (rng() % 100 < 25) {  // 25% chance
            std::uniform_int_distribution<int> offsetDist(-1, 1);
            int dx = offsetDist(rng);
            int dy = offsetDist(rng);
            
            if (dx != 0 || dy != 0) {  // Don't place on self
                Position adjacent(pos.x + dx, pos.y + dy);
                if (isValidPosition(adjacent) && grid[adjacent.y][adjacent.x].type == CellType::EMPTY) {
                    grid[adjacent.y][adjacent.x] = Cell(type);
                    placed++;
                }
            }
        }
    }
}

/**
 * @brief Place warehouses for both teams in strategic locations
 */
void Map::placeWarehouses() {
    // Blue team warehouses (left side)
    blueAmmoWarehouse = Position(3, GRID_HEIGHT / 2 - 2);
    blueMedicineWarehouse = Position(3, GRID_HEIGHT / 2 + 2);
    
    // Orange team warehouses (right side)
    orangeAmmoWarehouse = Position(GRID_WIDTH - 4, GRID_HEIGHT / 2 - 2);
    orangeMedicineWarehouse = Position(GRID_WIDTH - 4, GRID_HEIGHT / 2 + 2);
    
    // Clear area around warehouses and mark them
    auto clearAndMark = [this](Position pos, WarehouseType type) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                Position clearPos(pos.x + dx, pos.y + dy);
                if (isValidPosition(clearPos)) {
                    grid[clearPos.y][clearPos.x] = Cell(CellType::EMPTY);
                }
            }
        }
        grid[pos.y][pos.x] = Cell(CellType::WAREHOUSE, type);
    };
    
    clearAndMark(blueAmmoWarehouse, WarehouseType::AMMO);
    clearAndMark(blueMedicineWarehouse, WarehouseType::MEDICINE);
    clearAndMark(orangeAmmoWarehouse, WarehouseType::AMMO);
    clearAndMark(orangeMedicineWarehouse, WarehouseType::MEDICINE);
    
    // CRITICAL: Clear wider area around warehouses to ensure accessibility
    // Clear 5x5 area around each warehouse instead of just 3x3
    auto clearWideArea = [this](Position pos) {
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                Position clearPos(pos.x + dx, pos.y + dy);
                if (isValidPosition(clearPos)) {
                    grid[clearPos.y][clearPos.x] = Cell(CellType::EMPTY);
                }
            }
        }
    };
    
    clearWideArea(blueAmmoWarehouse);
    clearWideArea(blueMedicineWarehouse);
    clearWideArea(orangeAmmoWarehouse);
    clearWideArea(orangeMedicineWarehouse);
    
    // Re-mark warehouses after clearing (they might have been cleared)
    grid[blueAmmoWarehouse.y][blueAmmoWarehouse.x] = Cell(CellType::WAREHOUSE, WarehouseType::AMMO);
    grid[blueMedicineWarehouse.y][blueMedicineWarehouse.x] = Cell(CellType::WAREHOUSE, WarehouseType::MEDICINE);
    grid[orangeAmmoWarehouse.y][orangeAmmoWarehouse.x] = Cell(CellType::WAREHOUSE, WarehouseType::AMMO);
    grid[orangeMedicineWarehouse.y][orangeMedicineWarehouse.x] = Cell(CellType::WAREHOUSE, WarehouseType::MEDICINE);
    
    // CRITICAL: Clear spawn areas for all units to prevent blocking
    // Blue team spawn area (x=5-7, y=11-19)
    for (int y = 11; y <= 19; ++y) {
        for (int x = 5; x <= 7; ++x) {
            Position pos(x, y);
            if (isValidPosition(pos)) {
                grid[pos.y][pos.x] = Cell(CellType::EMPTY);
            }
        }
    }
    
    // Orange team spawn area (x=32-34, y=11-19)
    for (int y = 11; y <= 19; ++y) {
        for (int x = 32; x <= 34; ++x) {
            Position pos(x, y);
            if (isValidPosition(pos)) {
                grid[pos.y][pos.x] = Cell(CellType::EMPTY);
            }
        }
    }
}

/**
 * @brief Get cell at specified position
 */
const Cell& Map::getCell(const Position& pos) const {
    return grid[pos.y][pos.x];
}

/**
 * @brief Get cell at specified coordinates
 */
const Cell& Map::getCell(int x, int y) const {
    return grid[y][x];
}

/**
 * @brief Check if a position is passable
 */
bool Map::isPassable(const Position& pos) const {
    if (!isValidPosition(pos)) return false;
    return grid[pos.y][pos.x].isPassable();
}

/**
 * @brief Check if a position blocks sight
 */
bool Map::blocksSight(const Position& pos) const {
    if (!isValidPosition(pos)) return true;
    return grid[pos.y][pos.x].blocksSight();
}

/**
 * @brief Get warehouse position for a team
 */
Position Map::getWarehouse(Team team, WarehouseType type) const {
    if (team == Team::BLUE) {
        return (type == WarehouseType::AMMO) ? blueAmmoWarehouse : blueMedicineWarehouse;
    } else {
        return (type == WarehouseType::AMMO) ? orangeAmmoWarehouse : orangeMedicineWarehouse;
    }
}

/**
 * @brief Check if position is a warehouse
 */
bool Map::isWarehouse(const Position& pos) const {
    if (!isValidPosition(pos)) return false;
    return grid[pos.y][pos.x].type == CellType::WAREHOUSE;
}

/**
 * @brief Get warehouse type at position
 */
WarehouseType Map::getWarehouseType(const Position& pos) const {
    if (!isValidPosition(pos)) return WarehouseType::NONE;
    return grid[pos.y][pos.x].warehouseType;
}

/**
 * @brief Reset the map to initial state
 */
void Map::reset() {
    grid.clear();
    grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
    generateObstacles();
    placeWarehouses();
}
