#include "map.h"
#include <random>
#include <ctime>
#include <fstream>
#include <iomanip>

/**
 * @brief Construct a new Map object and generate terrain
 */
Map::Map() {
    // Initialize grid with empty cells
    grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
    
    // Initialize warehouse inventories
    // OPTION 1: 2x capacity for extended battles (was 1.5x)
    // Enough supplies for 100+ turn battles without depletion crisis
    blueAmmoInventory.ammo = 450;         // 450 bullets (2x base 225) - ~30 resupplies
    blueAmmoInventory.grenades = 90;      // 90 grenades (2x base 45) - ~45 resupplies
    blueMedicineInventory.medicine = 1500; // 1500 medicine (2x base 750) - ~15 full heals
    
    orangeAmmoInventory.ammo = 450;
    orangeAmmoInventory.grenades = 90;
    orangeMedicineInventory.medicine = 1500;
    
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
 * @brief Get which team owns a warehouse at this position
 */
Team Map::getWarehouseTeam(const Position& pos) const {
    if (!isWarehouse(pos)) return Team::BLUE;  // Default
    
    if (pos == blueAmmoWarehouse || pos == blueMedicineWarehouse) {
        return Team::BLUE;
    } else {
        return Team::ORANGE;
    }
}

/**
 * @brief Try to take ammo from warehouse inventory
 */
bool Map::takeAmmo(Team team, int amount) {
    WarehouseInventory& inventory = (team == Team::BLUE) ? blueAmmoInventory : orangeAmmoInventory;
    
    if (inventory.ammo >= amount) {
        inventory.ammo -= amount;
        return true;
    }
    return false;  // Not enough ammo
}

/**
 * @brief Try to take grenades from warehouse inventory
 */
bool Map::takeGrenades(Team team, int amount) {
    WarehouseInventory& inventory = (team == Team::BLUE) ? blueAmmoInventory : orangeAmmoInventory;
    
    if (inventory.grenades >= amount) {
        inventory.grenades -= amount;
        return true;
    }
    return false;  // Not enough grenades
}

/**
 * @brief Try to take medicine from warehouse inventory
 */
bool Map::takeMedicine(Team team, int amount) {
    WarehouseInventory& inventory = (team == Team::BLUE) ? blueMedicineInventory : orangeMedicineInventory;
    
    if (inventory.medicine >= amount) {
        inventory.medicine -= amount;
        return true;
    }
    return false;  // Not enough medicine
}

/**
 * @brief Get current ammo inventory
 */
int Map::getAmmoInventory(Team team) const {
    return (team == Team::BLUE) ? blueAmmoInventory.ammo : orangeAmmoInventory.ammo;
}

/**
 * @brief Get current grenade inventory
 */
int Map::getGrenadeInventory(Team team) const {
    return (team == Team::BLUE) ? blueAmmoInventory.grenades : orangeAmmoInventory.grenades;
}

/**
 * @brief Get current medicine inventory
 */
int Map::getMedicineInventory(Team team) const {
    return (team == Team::BLUE) ? blueMedicineInventory.medicine : orangeMedicineInventory.medicine;
}

/**
 * @brief Reset the map to initial state
 */
void Map::reset() {
    grid.clear();
    grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
    
    // Reset inventories - OPTION 1: 2x capacity
    blueAmmoInventory.ammo = 450;
    blueAmmoInventory.grenades = 90;
    blueMedicineInventory.medicine = 1500;
    orangeAmmoInventory.ammo = 450;
    orangeAmmoInventory.grenades = 90;
    orangeMedicineInventory.medicine = 1500;
    
    generateObstacles();
    placeWarehouses();
}

/**
 * @brief Log the map layout to a separate file for debugging
 */
void Map::logMapLayout() const {
    std::ofstream logFile("logs/map_layout.txt");
    if (!logFile.is_open()) {
        return;
    }
    
    logFile << "=================================================\n";
    logFile << "  Map Layout - " << GRID_WIDTH << "x" << GRID_HEIGHT << " Grid\n";
    logFile << "=================================================\n\n";
    
    // Legend
    logFile << "Legend:\n";
    logFile << "  . = Empty terrain\n";
    logFile << "  # = Rock (blocks movement, sight, shooting)\n";
    logFile << "  T = Tree (allows movement, blocks sight/shooting)\n";
    logFile << "  ~ = Water (blocks movement, allows sight/shooting)\n";
    logFile << "  BA = Blue Ammo Warehouse\n";
    logFile << "  BM = Blue Medicine Warehouse\n";
    logFile << "  OA = Orange Ammo Warehouse\n";
    logFile << "  OM = Orange Medicine Warehouse\n\n";
    
    // Column numbers (top)
    logFile << "     ";
    for (int x = 0; x < GRID_WIDTH; x++) {
        logFile << (x % 10);
    }
    logFile << "\n";
    
    logFile << "     ";
    for (int x = 0; x < GRID_WIDTH; x++) {
        logFile << "-";
    }
    logFile << "\n";
    
    // Map grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        // Row number (left)
        logFile << std::setw(3) << y << " |";
        
        for (int x = 0; x < GRID_WIDTH; x++) {
            Position pos(x, y);
            
            // Check if warehouse
            if (pos == blueAmmoWarehouse) {
                logFile << "B";
            } else if (pos == blueMedicineWarehouse) {
                logFile << "M";
            } else if (pos == orangeAmmoWarehouse) {
                logFile << "O";
            } else if (pos == orangeMedicineWarehouse) {
                logFile << "X";
            } else {
                // Check terrain type
                CellType type = grid[y][x].type;
                switch (type) {
                    case CellType::EMPTY:
                        logFile << ".";
                        break;
                    case CellType::ROCK:
                        logFile << "#";
                        break;
                    case CellType::TREE:
                        logFile << "T";
                        break;
                    case CellType::WATER:
                        logFile << "~";
                        break;
                    default:
                        logFile << "?";
                        break;
                }
            }
        }
        
        logFile << "| " << y << "\n";
    }
    
    // Column numbers (bottom)
    logFile << "     ";
    for (int x = 0; x < GRID_WIDTH; x++) {
        logFile << "-";
    }
    logFile << "\n";
    
    logFile << "     ";
    for (int x = 0; x < GRID_WIDTH; x++) {
        logFile << (x % 10);
    }
    logFile << "\n\n";
    
    // Warehouse positions
    logFile << "Warehouse Positions:\n";
    logFile << "  Blue Ammo:     (" << blueAmmoWarehouse.x << "," << blueAmmoWarehouse.y << ")\n";
    logFile << "  Blue Medicine: (" << blueMedicineWarehouse.x << "," << blueMedicineWarehouse.y << ")\n";
    logFile << "  Orange Ammo:   (" << orangeAmmoWarehouse.x << "," << orangeAmmoWarehouse.y << ")\n";
    logFile << "  Orange Medicine: (" << orangeMedicineWarehouse.x << "," << orangeMedicineWarehouse.y << ")\n\n";
    
    // Inventory status
    logFile << "Warehouse Inventories:\n";
    logFile << "  Blue Ammo:     " << blueAmmoInventory.ammo << " bullets, " 
            << blueAmmoInventory.grenades << " grenades\n";
    logFile << "  Blue Medicine: " << blueMedicineInventory.medicine << " medicine\n";
    logFile << "  Orange Ammo:   " << orangeAmmoInventory.ammo << " bullets, " 
            << orangeAmmoInventory.grenades << " grenades\n";
    logFile << "  Orange Medicine: " << orangeMedicineInventory.medicine << " medicine\n\n";
    
    // Terrain statistics
    int emptyCount = 0, rockCount = 0, treeCount = 0, waterCount = 0;
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            switch (grid[y][x].type) {
                case CellType::EMPTY: emptyCount++; break;
                case CellType::ROCK: rockCount++; break;
                case CellType::TREE: treeCount++; break;
                case CellType::WATER: waterCount++; break;
            }
        }
    }
    
    int totalCells = GRID_WIDTH * GRID_HEIGHT;
    logFile << "Terrain Statistics:\n";
    logFile << "  Empty:  " << emptyCount << " (" << (emptyCount * 100 / totalCells) << "%)\n";
    logFile << "  Rocks:  " << rockCount << " (" << (rockCount * 100 / totalCells) << "%)\n";
    logFile << "  Trees:  " << treeCount << " (" << (treeCount * 100 / totalCells) << "%)\n";
    logFile << "  Water:  " << waterCount << " (" << (waterCount * 100 / totalCells) << "%)\n";
    logFile << "  Total:  " << totalCells << " cells\n";
    
    logFile.close();
}
