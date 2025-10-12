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
 * @brief Generate obstacle clusters (rocks, trees, water) on the map
 */
void Map::generateObstacles() {
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_int_distribution<int> xDist(5, GRID_WIDTH - 6);
    std::uniform_int_distribution<int> yDist(5, GRID_HEIGHT - 6);
    std::uniform_int_distribution<int> typeDist(0, 2);
    std::uniform_int_distribution<int> radiusDist(2, 4);
    
    // Create 8-12 obstacle clusters
    int numClusters = 8 + (rng() % 5);
    
    for (int i = 0; i < numClusters; ++i) {
        Position center(xDist(rng), yDist(rng));
        int radius = radiusDist(rng);
        
        CellType type;
        int typeChoice = typeDist(rng);
        if (typeChoice == 0) type = CellType::ROCK;
        else if (typeChoice == 1) type = CellType::TREE;
        else type = CellType::WATER;
        
        createObstacleCluster(center, type, radius);
    }
}

/**
 * @brief Create a cluster of obstacles around a center point
 * 
 * @param center Center position of the cluster
 * @param type Type of obstacle to place
 * @param radius Approximate radius of the cluster
 */
void Map::createObstacleCluster(Position center, CellType type, int radius) {
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)) + center.x * 1000 + center.y);
    std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
    
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            Position pos(center.x + dx, center.y + dy);
            
            if (!isValidPosition(pos)) continue;
            
            // Distance-based probability (closer to center = higher chance)
            float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            float probability = 1.0f - (distance / (radius + 1));
            
            if (probDist(rng) < probability) {
                grid[pos.y][pos.x] = Cell(type);
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
