#include "simulation.h"
#include <sstream>
#include <iomanip>

/**
 * @brief Construct simulation and initialize game
 */
Simulation::Simulation()
    : currentTurn(0), gameOver(false), winner(Team::BLUE), isDraw(false),
      paused(false), turnDelay(0.5f), timeSinceLastTurn(0.0f), gameOverTimer(0.0f), showFogOfWar(true),
      showVisionCones(false), showWeaponRanges(false),
      stalemateType(""), finalBlueWarriors(0), finalOrangeWarriors(0), 
      finalBlueHP(0), finalOrangeHP(0), finalBlueScore(0), finalOrangeScore(0),
      turnsAtWarehouse(0), blueWarriorAtWarehouse(false), orangeWarriorAtWarehouse(false),
      lastBlueWarriorPos(-1, -1), lastOrangeWarriorPos(-1, -1), turnsWithoutMovement(0) {
    initializeTeams();
    
    // Log the map layout to a separate file for debugging
    gameMap.logMapLayout();
}

/**
 * @brief Destructor - clean up characters
 */
Simulation::~Simulation() {
    for (Character* c : allCharacters) {
        delete c;
    }
    allCharacters.clear();
    blueTeam.clear();
    orangeTeam.clear();
}

/**
 * @brief Initialize both teams with starting positions
 */
void Simulation::initializeTeams() {
    // Clear existing characters
    for (Character* c : allCharacters) {
        delete c;
    }
    allCharacters.clear();
    blueTeam.clear();
    orangeTeam.clear();
    
    // Blue team (left side) - positions near their warehouses
    int blueX = 5;
    int startY = GRID_HEIGHT / 2;
    
    Commander* blueCommander = new Commander(Position(blueX, startY), Team::BLUE);
    Warrior* blueWarrior1 = new Warrior(Position(blueX + 2, startY - 2), Team::BLUE);
    Warrior* blueWarrior2 = new Warrior(Position(blueX + 2, startY + 2), Team::BLUE);
    Medic* blueMedic = new Medic(Position(blueX + 1, startY + 4), Team::BLUE);
    Supplier* blueSupplier = new Supplier(Position(blueX + 1, startY - 4), Team::BLUE);
    
    blueTeam.push_back(blueCommander);
    blueTeam.push_back(blueWarrior1);
    blueTeam.push_back(blueWarrior2);
    blueTeam.push_back(blueMedic);
    blueTeam.push_back(blueSupplier);
    
    // Orange team (right side)
    int orangeX = GRID_WIDTH - 6;
    
    Commander* orangeCommander = new Commander(Position(orangeX, startY), Team::ORANGE);
    Warrior* orangeWarrior1 = new Warrior(Position(orangeX - 2, startY - 2), Team::ORANGE);
    Warrior* orangeWarrior2 = new Warrior(Position(orangeX - 2, startY + 2), Team::ORANGE);
    Medic* orangeMedic = new Medic(Position(orangeX - 1, startY + 4), Team::ORANGE);
    Supplier* orangeSupplier = new Supplier(Position(orangeX - 1, startY - 4), Team::ORANGE);
    
    orangeTeam.push_back(orangeCommander);
    orangeTeam.push_back(orangeWarrior1);
    orangeTeam.push_back(orangeWarrior2);
    orangeTeam.push_back(orangeMedic);
    orangeTeam.push_back(orangeSupplier);
    
    // Add all to main list
    for (Character* c : blueTeam) allCharacters.push_back(c);
    for (Character* c : orangeTeam) allCharacters.push_back(c);
}

/**
 * @brief Update simulation state
 */
void Simulation::update(float deltaTime) {
    // If game is over, wait 5 seconds then exit
    if (gameOver) {
        gameOverTimer += deltaTime;
        if (gameOverTimer >= 5.0f) {
            LOG_CONTROL("\n=== AUTO-EXITING AFTER GAME OVER ===\n");
            Logger::shutdown();
            exit(0);
        }
        return;
    }
    
    if (paused) return;
    
    timeSinceLastTurn += deltaTime;
    
    if (timeSinceLastTurn >= turnDelay) {
        timeSinceLastTurn = 0.0f;
        currentTurn++;
        
        updateAllCharacters();
        checkVictoryConditions();
        checkStalemateConditions();  // Check for warehouse stalemate
    }
}

/**
 * @brief Update all characters
 */
void Simulation::updateAllCharacters() {
    // Log turn number for easier debugging
    LOG_CHARACTER("\n============================================\n");
    LOG_CHARACTER("TURN " << currentTurn << "\n");
    LOG_CHARACTER("============================================\n");
    
    // Update commanders first (they issue orders)
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getType() == CharacterType::COMMANDER) {
            c->update(gameMap, allCharacters, currentTurn);
        }
    }
    
    // Then update other characters
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getType() != CharacterType::COMMANDER) {
            c->update(gameMap, allCharacters, currentTurn);
            
            // Check for visual effects (shots and grenades)
            if (c->getType() == CharacterType::WARRIOR) {
                Warrior* warrior = static_cast<Warrior*>(c);
                if (warrior->didShootThisTurn()) {
                    addShootEffect(warrior->getPosition(), warrior->getLastShotTarget(), false, warrior->getTeam());
                }
                if (warrior->didThrowGrenadeThisTurn()) {
                    addShootEffect(warrior->getPosition(), warrior->getLastGrenadeTarget(), true, warrior->getTeam());
                }
            }
        }
    }
}

/**
 * @brief Check if game is over
 */
void Simulation::checkVictoryConditions() {
    // Victory is determined by warriors only - support units can't win alone
    int blueWarriors = 0;
    int orangeWarriors = 0;
    int blueHP = 0, orangeHP = 0;
    
    for (Character* c : allCharacters) {
        if (c->isAlive()) {
            if (c->getType() == CharacterType::WARRIOR) {
                if (c->getTeam() == Team::BLUE) {
                    blueWarriors++;
                    blueHP += c->getHealth();
                } else {
                    orangeWarriors++;
                    orangeHP += c->getHealth();
                }
            } else if (c->getType() != CharacterType::COMMANDER) {
                // Count support unit HP too
                if (c->getTeam() == Team::BLUE) {
                    blueHP += c->getHealth();
                } else {
                    orangeHP += c->getHealth();
                }
            }
        }
    }
    
    if (blueWarriors == 0 && orangeWarriors > 0) {
        gameOver = true;
        winner = Team::ORANGE;
        isDraw = false;
        stalemateType = "";  // Normal victory, not stalemate
        
        // Store final stats
        finalBlueWarriors = blueWarriors;
        finalOrangeWarriors = orangeWarriors;
        finalBlueHP = blueHP;
        finalOrangeHP = orangeHP;
        finalBlueScore = blueWarriors * 300 + blueHP;
        finalOrangeScore = orangeWarriors * 300 + orangeHP;
        
        LOG_CONTROL("=== GAME OVER: Orange team wins! ===\n");
    } else if (orangeWarriors == 0 && blueWarriors > 0) {
        gameOver = true;
        winner = Team::BLUE;
        isDraw = false;
        stalemateType = "";  // Normal victory, not stalemate
        
        // Store final stats
        finalBlueWarriors = blueWarriors;
        finalOrangeWarriors = orangeWarriors;
        finalBlueHP = blueHP;
        finalOrangeHP = orangeHP;
        finalBlueScore = blueWarriors * 300 + blueHP;
        finalOrangeScore = orangeWarriors * 300 + orangeHP;
        
        LOG_CONTROL("=== GAME OVER: Blue team wins! ===\n");
    } else if (blueWarriors == 0 && orangeWarriors == 0) {
        gameOver = true;
        winner = Team::BLUE;  // Default, but isDraw = true
        isDraw = true;
        stalemateType = "ALL WARRIORS ELIMINATED";
        
        // Store final stats
        finalBlueWarriors = 0;
        finalOrangeWarriors = 0;
        finalBlueHP = blueHP;
        finalOrangeHP = orangeHP;
        finalBlueScore = blueHP;
        finalOrangeScore = orangeHP;
        
        LOG_CONTROL("=== GAME OVER: Draw! All warriors eliminated. ===\n");
    }
}

/**
 * @brief Determine winner in stalemate by scoring system
 * Warriors count more (300 points each), then total HP as tiebreaker
 */
Team Simulation::determineWinnerByScore() {
    int blueWarriors = 0, orangeWarriors = 0;
    int blueTotalHP = 0, orangeTotalHP = 0;
    
    for (Character* c : allCharacters) {
        if (!c->isAlive()) continue;
        
        if (c->getType() == CharacterType::WARRIOR) {
            if (c->getTeam() == Team::BLUE) {
                blueWarriors++;
                blueTotalHP += c->getHealth();
            } else {
                orangeWarriors++;
                orangeTotalHP += c->getHealth();
            }
        } else if (c->getType() != CharacterType::COMMANDER) {
            // Support units count for HP but not as heavily
            if (c->getTeam() == Team::BLUE) {
                blueTotalHP += c->getHealth();
            } else {
                orangeTotalHP += c->getHealth();
            }
        }
    }
    
    // Calculate score: Warriors are worth 300 points each, then HP as tiebreaker
    int blueScore = blueWarriors * 300 + blueTotalHP;
    int orangeScore = orangeWarriors * 300 + orangeTotalHP;
    
    // Store stats for display
    finalBlueWarriors = blueWarriors;
    finalOrangeWarriors = orangeWarriors;
    finalBlueHP = blueTotalHP;
    finalOrangeHP = orangeTotalHP;
    finalBlueScore = blueScore;
    finalOrangeScore = orangeScore;
    
    LOG_CONTROL("║   SCORE CALCULATION:                              ║\n");
    LOG_CONTROL("║   Blue:   " << blueWarriors << " warriors × 300 + " << blueTotalHP 
               << " HP = " << blueScore << " points\n");
    LOG_CONTROL("║   Orange: " << orangeWarriors << " warriors × 300 + " << orangeTotalHP 
               << " HP = " << orangeScore << " points\n");
    
    if (blueScore > orangeScore) {
        LOG_CONTROL("║   Winner: Blue Team (by score)                    ║\n");
        return Team::BLUE;
    } else if (orangeScore > blueScore) {
        LOG_CONTROL("║   Winner: Orange Team (by score)                  ║\n");
        return Team::ORANGE;
    } else {
        LOG_CONTROL("║   Result: True Draw (equal scores)                ║\n");
        return Team::BLUE;  // True draw, default to Blue (caller sets isDraw)
    }
}

/**
 * @brief Check for stalemate conditions (warriors stuck at warehouses)
 */
void Simulation::checkStalemateConditions() {
    if (gameOver) return;  // Already game over
    
    // Find ALL alive warriors for each team
    std::vector<Warrior*> blueWarriors;
    std::vector<Warrior*> orangeWarriors;
    
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getType() == CharacterType::WARRIOR) {
            Warrior* w = dynamic_cast<Warrior*>(c);
            if (w) {
                if (w->getTeam() == Team::BLUE) {
                    blueWarriors.push_back(w);
                } else {
                    orangeWarriors.push_back(w);
                }
            }
        }
    }
    
    // Need at least one warrior per team to check stalemate
    if (blueWarriors.empty() || orangeWarriors.empty()) {
        return;  // Victory condition will handle this
    }
    
    // STALEMATE CHECK 1: All warriors at warehouses (multiple warriors per team)
    bool allAtWarehouses = true;
    for (Warrior* w : blueWarriors) {
        if (!gameMap.isWarehouse(w->getPosition())) {
            allAtWarehouses = false;
            break;
        }
    }
    if (allAtWarehouses) {
        for (Warrior* w : orangeWarriors) {
            if (!gameMap.isWarehouse(w->getPosition())) {
                allAtWarehouses = false;
                break;
            }
        }
    }
    
    if (allAtWarehouses) {
        turnsAtWarehouse++;
        
        if (turnsAtWarehouse >= 20) {
            gameOver = true;
            stalemateType = "WAREHOUSE DEADLOCK";
            LOG_CONTROL("\n╔════════════════════════════════════════════════════╗\n");
            LOG_CONTROL("║       STALEMATE - WAREHOUSE DEADLOCK              ║\n");
            LOG_CONTROL("║   All warriors stuck at warehouses for 20 turns   ║\n");
            LOG_CONTROL("║   Blue warriors: " << blueWarriors.size() << " | Orange warriors: " << orangeWarriors.size() << "           \n");
            LOG_CONTROL("║   Support units unable to deliver supplies        ║\n");
            
            winner = determineWinnerByScore();
            isDraw = (finalBlueScore == finalOrangeScore);
            
            if (!isDraw) {
                LOG_CONTROL("║   Final State: " << teamToString(winner) << " WINS BY SCORE!        \n");
            } else {
                LOG_CONTROL("║   Final State: DRAW - EQUAL SCORES                ║\n");
            }
            LOG_CONTROL("╚════════════════════════════════════════════════════╝\n");
        }
    } else {
        turnsAtWarehouse = 0;
    }
    
    // STALEMATE CHECK 2: Warriors stuck in same positions (only if single warrior per team)
    if (blueWarriors.size() == 1 && orangeWarriors.size() == 1) {
        Warrior* blueWarrior = blueWarriors[0];
        Warrior* orangeWarrior = orangeWarriors[0];
        
        Position bluePos = blueWarrior->getPosition();
        Position orangePos = orangeWarrior->getPosition();
        
        // Check if positions haven't changed
        if (bluePos == lastBlueWarriorPos && orangePos == lastOrangeWarriorPos) {
            turnsWithoutMovement++;
            
            // Check if both have 0 ammo or are stuck in retreat/warehouse seeking
            bool blueStuck = (blueWarrior->getAmmo() == 0) || blueWarrior->getIsRetreating();
            bool orangeStuck = (orangeWarrior->getAmmo() == 0) || orangeWarrior->getIsRetreating();
            
            if (turnsWithoutMovement >= 30 && blueStuck && orangeStuck) {
                gameOver = true;
                stalemateType = "POSITIONAL DEADLOCK";
                LOG_CONTROL("\n╔════════════════════════════════════════════════════╗\n");
                LOG_CONTROL("║       STALEMATE - POSITIONAL DEADLOCK             ║\n");
                LOG_CONTROL("║   Both warriors stuck for 30 turns                ║\n");
                LOG_CONTROL("║   Blue at (" << bluePos.x << "," << bluePos.y << ") | HP:" << blueWarrior->getHealth() 
                       << " | Ammo:" << blueWarrior->getAmmo() << "              \n");
                LOG_CONTROL("║   Orange at (" << orangePos.x << "," << orangePos.y << ") | HP:" << orangeWarrior->getHealth() 
                       << " | Ammo:" << orangeWarrior->getAmmo() << "            \n");
                
                winner = determineWinnerByScore();
                isDraw = (finalBlueScore == finalOrangeScore);
                
                if (!isDraw) {
                    LOG_CONTROL("║   Final State: " << teamToString(winner) << " WINS BY SCORE!        \n");
                } else {
                    LOG_CONTROL("║   Final State: DRAW - EQUAL SCORES                ║\n");
                }
                LOG_CONTROL("╚════════════════════════════════════════════════════╝\n");
            }
        } else {
            // Reset counter if warriors moved
            turnsWithoutMovement = 0;
        }
        
        // Update last known positions
        lastBlueWarriorPos = bluePos;
        lastOrangeWarriorPos = orangePos;
    } else {
        // Multiple warriors - don't check positional stalemate
        turnsWithoutMovement = 0;
        lastBlueWarriorPos = Position(-1, -1);
        lastOrangeWarriorPos = Position(-1, -1);
    }
    
    // STALEMATE CHECK 3: All warriors in autonomous mode with no progress (50+ turns)
    // This catches situations where warriors can't see/shoot each other due to terrain
    bool allInAutonomous = true;
    for (Warrior* w : blueWarriors) {
        if (w->getAutonomousCooldown() == 0) {
            allInAutonomous = false;
            break;
        }
    }
    if (allInAutonomous) {
        for (Warrior* w : orangeWarriors) {
            if (w->getAutonomousCooldown() == 0) {
                allInAutonomous = false;
                break;
            }
        }
    }
    
    // Track turns in autonomous stalemate
    static int turnsAllAutonomous = 0;
    static std::unordered_map<Warrior*, int> lastDamageMap;
    
    if (allInAutonomous) {
        turnsAllAutonomous++;
        
        // Check if any warrior has taken/dealt damage recently
        bool anyRecentDamage = false;
        for (Warrior* w : blueWarriors) {
            int currentHealth = w->getHealth();
            if (lastDamageMap.find(w) == lastDamageMap.end()) {
                lastDamageMap[w] = currentHealth;
            } else if (lastDamageMap[w] != currentHealth) {
                anyRecentDamage = true;
                lastDamageMap[w] = currentHealth;
            }
        }
        for (Warrior* w : orangeWarriors) {
            int currentHealth = w->getHealth();
            if (lastDamageMap.find(w) == lastDamageMap.end()) {
                lastDamageMap[w] = currentHealth;
            } else if (lastDamageMap[w] != currentHealth) {
                anyRecentDamage = true;
                lastDamageMap[w] = currentHealth;
            }
        }
        
        // If no damage for 50 turns while all autonomous = stalemate
        if (turnsAllAutonomous >= 50 && !anyRecentDamage) {
            gameOver = true;
            stalemateType = "AUTONOMOUS MODE DEADLOCK";
            LOG_CONTROL("\n╔════════════════════════════════════════════════════╗\n");
            LOG_CONTROL("║       STALEMATE - AUTONOMOUS MODE DEADLOCK        ║\n");
            LOG_CONTROL("║   All warriors in autonomous mode for 50+ turns   ║\n");
            LOG_CONTROL("║   No combat progress - likely LOS blocked         ║\n");
            LOG_CONTROL("║   Blue warriors: " << blueWarriors.size() << " | Orange warriors: " << orangeWarriors.size() << "           \n");
            
            winner = determineWinnerByScore();
            isDraw = (finalBlueScore == finalOrangeScore);
            
            if (!isDraw) {
                LOG_CONTROL("║   Final State: " << teamToString(winner) << " WINS BY SCORE!        \n");
            } else {
                LOG_CONTROL("║   Final State: DRAW - EQUAL SCORES                ║\n");
            }
            LOG_CONTROL("╚════════════════════════════════════════════════════╝\n");
            turnsAllAutonomous = 0;
            lastDamageMap.clear();
        }
    } else {
        turnsAllAutonomous = 0;
        lastDamageMap.clear();
    }
    
    // STALEMATE CHECK 4: No enemy sightings for extended period (fog of war deadlock)
    // If warriors haven't seen enemies for 50+ turns despite enemies being alive = stalemate
    static int turnsWithoutSighting = 0;
    static std::unordered_map<Warrior*, int> lastTurnSawEnemy;
    
    bool anyWarriorSeesEnemy = false;
    for (Warrior* w : blueWarriors) {
        // Check if warrior can ACTUALLY SEE any enemy warrior (not just distance)
        for (Warrior* enemyW : orangeWarriors) {
            // Use canSee() to check line-of-sight, not just distance
            if (w->canSee(enemyW->getPosition())) {
                anyWarriorSeesEnemy = true;
                lastTurnSawEnemy[w] = currentTurn;
                break;
            }
        }
    }
    for (Warrior* w : orangeWarriors) {
        // Check if warrior can ACTUALLY SEE any enemy warrior (not just distance)
        for (Warrior* enemyW : blueWarriors) {
            // Use canSee() to check line-of-sight, not just distance
            if (w->canSee(enemyW->getPosition())) {
                anyWarriorSeesEnemy = true;
                lastTurnSawEnemy[w] = currentTurn;
                break;
            }
        }
    }
    
    if (!anyWarriorSeesEnemy) {
        turnsWithoutSighting++;
        
        // Stalemate if no sightings for 50 turns
        if (turnsWithoutSighting >= 50) {
            gameOver = true;
            stalemateType = "FOG OF WAR DEADLOCK";
            LOG_CONTROL("\n╔════════════════════════════════════════════════════╗\n");
            LOG_CONTROL("║       STALEMATE - FOG OF WAR DEADLOCK             ║\n");
            LOG_CONTROL("║   No enemy sightings for 50+ turns                ║\n");
            LOG_CONTROL("║   Warriors too far apart / blocked by terrain     ║\n");
            LOG_CONTROL("║   Blue warriors: " << blueWarriors.size() << " | Orange warriors: " << orangeWarriors.size() << "           \n");
            
            winner = determineWinnerByScore();
            isDraw = (finalBlueScore == finalOrangeScore);
            
            if (!isDraw) {
                LOG_CONTROL("║   Final State: " << teamToString(winner) << " WINS BY SCORE!        \n");
            } else {
                LOG_CONTROL("║   Final State: DRAW - EQUAL SCORES                ║\n");
            }
            LOG_CONTROL("╚════════════════════════════════════════════════════╝\n");
            turnsWithoutSighting = 0;
            lastTurnSawEnemy.clear();
        }
    } else {
        turnsWithoutSighting = 0;
    }
}

/**
 * @brief Count alive characters for a team
 */
int Simulation::countAliveCharacters(Team team) {
    int count = 0;
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getTeam() == team) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Get team with alive characters
 */
Team Simulation::getAliveTeam() {
    for (Character* c : allCharacters) {
        if (c->isAlive()) {
            return c->getTeam();
        }
    }
    return Team::BLUE;
}

/**
 * @brief Reset simulation
 */
void Simulation::reset() {
    gameMap.reset();
    currentTurn = 0;
    gameOver = false;
    paused = false;
    timeSinceLastTurn = 0.0f;
    initializeTeams();
    
    // Log the new map layout after reset
    gameMap.logMapLayout();
}

/**
 * @brief Check if a position is occupied by another character
 */
bool Simulation::isPositionOccupied(const Position& pos, const Character* excludeChar) const {
    for (const Character* c : allCharacters) {
        if (c->isAlive() && c != excludeChar && c->getPosition() == pos) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Render the entire simulation
 */
void Simulation::render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    
    renderGrid();
    
    // Render order lines (under characters)
    renderOrderLines();
    
    // Render all characters
    for (Character* c : allCharacters) {
        if (c->isAlive()) {
            renderCharacter(c);
        }
    }
    
    // Render fog of war if enabled
    if (showFogOfWar) {
        renderFogOfWar();
    }
    
    // Render vision cones if enabled
    if (showVisionCones) {
        renderVisionCones();
    }
    
    // Render weapon ranges if enabled
    if (showWeaponRanges) {
        renderWeaponRanges();
    }
    
    // Render shooting effects (over characters)
    renderEffects();
    
    renderUI();
    
    glutSwapBuffers();
}

/**
 * @brief Render the grid and obstacles
 */
void Simulation::renderGrid() {
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            renderCell(x, y);
        }
    }
}

/**
 * @brief Render a single cell
 */
void Simulation::renderCell(int x, int y) {
    float screenX = x * CELL_SIZE;
    float screenY = y * CELL_SIZE;
    
    const Cell& cell = gameMap.getCell(x, y);
    
    // Choose color based on cell type
    float r = 0.7f, g = 0.9f, b = 0.7f;  // Default: light green (empty grass)
    
    switch (cell.type) {
        case CellType::ROCK:
            r = 0.5f; g = 0.5f; b = 0.5f;  // Gray stones
            drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
            break;
        case CellType::TREE:
            // Light green grass background
            drawSquare(screenX, screenY, CELL_SIZE - 1, 0.7f, 0.9f, 0.7f);
            // Dark green triangle for tree
            drawTriangle(screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2, CELL_SIZE - 4, 0.1f, 0.5f, 0.1f);
            return;  // Skip the default square drawing
        case CellType::WATER:
            r = 0.6f; g = 0.8f; b = 1.0f;  // Light blue water
            drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
            break;
        case CellType::WAREHOUSE: {
            // Dark yellow warehouses with slight team tint
            Team warehouseTeam = gameMap.getWarehouseTeam(Position{x, y});
            WarehouseType warehouseType = gameMap.getWarehouseType(Position{x, y});
            
            if (warehouseTeam == Team::BLUE) {
                r = 0.6f; g = 0.6f; b = 0.1f;  // Dark yellow with blue tint
            } else {
                r = 0.7f; g = 0.6f; b = 0.1f;  // Dark yellow with orange tint
            }
            drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
            
            // Draw icon to indicate warehouse type (M=Medicine, A=Ammo)
            glColor3f(0.0f, 0.0f, 0.0f);  // Black text for better readability
            glRasterPos2f(screenX + CELL_SIZE/3, screenY + CELL_SIZE/2);
            
            if (warehouseType == WarehouseType::MEDICINE) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'M');
                
                // Show medicine inventory
                int inventory = gameMap.getMedicineInventory(warehouseTeam);
                std::string invText = std::to_string(inventory);
                drawText(screenX + 2, screenY - 8, invText.c_str(), 0.0f, 0.0f, 0.0f);  // Black text
            } else if (warehouseType == WarehouseType::AMMO) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'A');
                
                // Show ammo inventory (ammo + grenades)
                int ammo = gameMap.getAmmoInventory(warehouseTeam);
                int grenades = gameMap.getGrenadeInventory(warehouseTeam);
                std::string invText = std::to_string(ammo) + "/" + std::to_string(grenades);
                drawText(screenX - 2, screenY - 8, invText.c_str(), 0.0f, 0.0f, 0.0f);  // Black text
            }
            break;
        }
        default:
            drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
            break;
    }
}

/**
 * @brief Render a character
 */
void Simulation::renderCharacter(Character* character) {
    Position pos = character->getPosition();
    float screenX = pos.x * CELL_SIZE;
    float screenY = pos.y * CELL_SIZE;
    
    // Choose team color
    float r, g, b;
    if (character->getTeam() == Team::BLUE) {
        r = 0.2f; g = 0.4f; b = 0.9f;  // Blue
    } else {
        r = 1.0f; g = 0.5f; b = 0.0f;  // Orange
    }
    
    // Draw character square
    drawSquare(screenX + 2, screenY + 2, CELL_SIZE - 4, r, g, b);
    
    // Draw character type letter in the center
    char typeChar = character->getTypeChar();
    drawCharInSquare(screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2, typeChar, 1.0f, 1.0f, 1.0f);
    
    // Draw health bar ABOVE the character square (floating health bar)
    float healthPercent = static_cast<float>(character->getHealth()) / static_cast<float>(INITIAL_HEALTH);
    float barWidth = (CELL_SIZE - 8) * healthPercent;
    float barHeight = 3.0f;  // Thin bar
    float barYPos = screenY + CELL_SIZE + 4;  // Position above character
    
    // Black background for health bar
    drawRectangle(screenX + 4, barYPos, CELL_SIZE - 8, barHeight, 0.0f, 0.0f, 0.0f);
    
    // Color-coded health bar
    if (healthPercent > 0.5f) {
        drawRectangle(screenX + 4, barYPos, barWidth, barHeight, 0.0f, 1.0f, 0.0f);  // Green
    } else if (healthPercent > 0.25f) {
        drawRectangle(screenX + 4, barYPos, barWidth, barHeight, 1.0f, 1.0f, 0.0f);  // Yellow
    } else {
        drawRectangle(screenX + 4, barYPos, barWidth, barHeight, 1.0f, 0.0f, 0.0f);  // Red
    }
    
    // Draw resource information below character based on type
    float textYPos = screenY - 10;  // Position below character
    
    if (character->getType() == CharacterType::WARRIOR) {
        Warrior* warrior = static_cast<Warrior*>(character);
        int ammo = warrior->getAmmo();
        int grenades = warrior->getGrenades();
        
        // Format: "15/15 G:2" (ammo/max grenades:count)
        std::string resourceText = std::to_string(ammo) + "/15 G:" + std::to_string(grenades);
        
        // Draw light background for dark text visibility (sized for full text)
        drawRectangle(screenX - 2, textYPos - 2, 48, 10, 0.9f, 0.9f, 0.9f);
        
        // Draw resource text in dark blue (high contrast on light background)
        drawText(screenX, textYPos, resourceText.c_str(), 0.0f, 0.0f, 0.5f);
    } else if (character->getType() == CharacterType::MEDIC) {
        Medic* medic = static_cast<Medic*>(character);
        int medicine = medic->getMedicineSupplies();
        std::string medText = "M:" + std::to_string(medicine);
        
        // Draw light background
        drawRectangle(screenX + 1, textYPos - 2, 18, 10, 0.9f, 0.9f, 0.9f);
        
        // Draw medicine in dark green
        drawText(screenX + 2, textYPos, medText.c_str(), 0.0f, 0.4f, 0.0f);
    } else if (character->getType() == CharacterType::SUPPLIER) {
        Supplier* supplier = static_cast<Supplier*>(character);
        int supplies = supplier->getAmmoSupplies();
        std::string supText = "A:" + std::to_string(supplies);
        
        // Draw light background
        drawRectangle(screenX + 1, textYPos - 2, 18, 10, 0.9f, 0.9f, 0.9f);
        
        // Draw ammo supplies in dark orange/brown
        drawText(screenX + 2, textYPos, supText.c_str(), 0.6f, 0.3f, 0.0f);
    }
}

/**
 * @brief Render UI overlay
 */
void Simulation::renderUI() {
    // Draw dark background panel at top for info text (taller for more info)
    drawRectangle(0.0f, WINDOW_HEIGHT - 60.0f, static_cast<float>(WINDOW_WIDTH), 60.0f, 0.1f, 0.1f, 0.15f);
    
    // Count units by type for each team
    int blueWarriors = 0, blueOther = 0;
    int orangeWarriors = 0, orangeOther = 0;
    for (Character* c : allCharacters) {
        if (!c->isAlive()) continue;
        if (c->getTeam() == Team::BLUE) {
            if (c->getType() == CharacterType::WARRIOR) blueWarriors++;
            else if (c->getType() != CharacterType::COMMANDER) blueOther++;
        } else {
            if (c->getType() == CharacterType::WARRIOR) orangeWarriors++;
            else if (c->getType() != CharacterType::COMMANDER) orangeOther++;
        }
    }
    
    // Draw info panel - Line 1: Turn and unit counts
    std::ostringstream oss1;
    oss1 << "Turn: " << currentTurn 
        << " | Blue: " << blueWarriors << "W + " << blueOther << "S"
        << " | Orange: " << orangeWarriors << "W + " << orangeOther << "S";
    
    if (paused) {
        oss1 << " | PAUSED";
    }
    
    if (gameOver) {
        if (isDraw) {
            oss1 << " | GAME OVER - DRAW (Stalemate)";
        } else {
            oss1 << " | GAME OVER - " << teamToString(winner) << " WINS!";
        }
    }
    
    drawText(10, WINDOW_HEIGHT - 15, oss1.str(), 1.0f, 1.0f, 1.0f);  // White text
    
    // Line 2: Legend for order lines
    drawText(10, WINDOW_HEIGHT - 35, "Order Lines: Red=Attack | Cyan=Defend | Magenta=Move | Green=Heal | Orange=Resupply", 0.7f, 0.7f, 0.7f);
    
    // Line 3: Shooting legend
    drawText(10, WINDOW_HEIGHT - 50, "W=Warrior M=Medic P=Supplier C=Commander | Gun shots show as colored lines, Grenades as red arcs", 0.6f, 0.6f, 0.6f);
    
    // Draw dark background panel at bottom for controls
    drawRectangle(0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH), 35.0f, 0.1f, 0.1f, 0.15f);
    
    // Draw controls with cyan text on dark background (professional look)
    std::ostringstream controls;
    controls << "Controls: SPACE=Pause | R=Reset | +/- Speed | F=Fog of War";
    if (showFogOfWar) {
        controls << " [ON]";
    }
    drawText(10, 15, controls.str(), 0.3f, 1.0f, 1.0f);  // Cyan
    
    // Draw large victory popup if game is over
    if (gameOver) {
        // Semi-transparent dark overlay
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f);  // 70% opacity black
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(WINDOW_WIDTH, 0);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
        glVertex2f(0, WINDOW_HEIGHT);
        glEnd();
        glDisable(GL_BLEND);
        
        // Victory popup box (centered) - BIGGER box for stats
        float boxWidth = 600.0f;
        float boxHeight = 350.0f;  // Always show stats
        float boxX = (WINDOW_WIDTH - boxWidth) / 2.0f;
        float boxY = (WINDOW_HEIGHT - boxHeight) / 2.0f;
        
        // Draw popup box background (team color or gray for draw)
        if (isDraw) {
            drawRectangle(boxX, boxY, boxWidth, boxHeight, 0.5f, 0.5f, 0.5f);  // Gray for draw
        } else if (winner == Team::BLUE) {
            drawRectangle(boxX, boxY, boxWidth, boxHeight, 0.2f, 0.4f, 0.8f);  // Blue
        } else {
            drawRectangle(boxX, boxY, boxWidth, boxHeight, 0.9f, 0.5f, 0.2f);  // Orange
        }
        
        // Draw border
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(4.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(boxX, boxY);
        glVertex2f(boxX + boxWidth, boxY);
        glVertex2f(boxX + boxWidth, boxY + boxHeight);
        glVertex2f(boxX, boxY + boxHeight);
        glEnd();
        
        // Draw title text (large, centered)
        std::string titleText = isDraw ? "STALEMATE!" : "VICTORY!";
        float titleX = isDraw ? (boxX + 190) : (boxX + 200);
        drawText(titleX, boxY + boxHeight - 50, titleText.c_str(), 1.0f, 1.0f, 1.0f);
        
        // Draw result text
        std::ostringstream resultText;
        if (isDraw) {
            resultText << "Game ended in a Draw";
        } else if (!stalemateType.empty()) {
            resultText << teamToString(winner) << " Wins by Score!";
        } else {
            resultText << teamToString(winner) << " Team Wins!";
        }
        drawText(boxX + 160, boxY + boxHeight - 90, resultText.str().c_str(), 1.0f, 1.0f, 1.0f);
        
        // Show detailed stats for ALL game endings
        // Victory type / reason
        if (!stalemateType.empty()) {
            std::ostringstream typeText;
            typeText << "Reason: " << stalemateType;
            drawText(boxX + 120, boxY + boxHeight - 130, typeText.str().c_str(), 0.85f, 0.85f, 0.85f);
        } else {
            std::string reasonText = "Reason: All Enemy Warriors Eliminated";
            drawText(boxX + 100, boxY + boxHeight - 130, reasonText.c_str(), 0.85f, 0.85f, 0.85f);
        }
        
        // Separator line
        drawText(boxX + 150, boxY + boxHeight - 155, "════════════════════", 0.7f, 0.7f, 0.7f);
        
        // Final statistics title
        drawText(boxX + 200, boxY + boxHeight - 180, "FINAL STATISTICS", 0.9f, 0.9f, 0.9f);
        
        // Blue team stats
        std::ostringstream blueStats;
        blueStats << "Blue:   " << finalBlueWarriors << " warriors × 300 + " 
                 << finalBlueHP << " HP = " << finalBlueScore << " points";
        drawText(boxX + 50, boxY + boxHeight - 210, blueStats.str().c_str(), 0.4f, 0.6f, 1.0f);  // Blue color
        
        // Orange team stats
        std::ostringstream orangeStats;
        orangeStats << "Orange: " << finalOrangeWarriors << " warriors × 300 + " 
                   << finalOrangeHP << " HP = " << finalOrangeScore << " points";
        drawText(boxX + 50, boxY + boxHeight - 240, orangeStats.str().c_str(), 1.0f, 0.6f, 0.3f);  // Orange color
        
        // Winner explanation (only if not a true draw)
        if (!isDraw) {
            std::ostringstream explanation;
            if (finalBlueWarriors != finalOrangeWarriors) {
                explanation << teamToString(winner) << " eliminated all enemy warriors!";
            } else if (!stalemateType.empty()) {
                // Stalemate - explain by HP
                explanation << teamToString(winner) << " had more total HP!";
            } else {
                explanation << teamToString(winner) << " dominated the battlefield!";
            }
            drawText(boxX + 140, boxY + boxHeight - 275, explanation.str().c_str(), 1.0f, 1.0f, 0.0f);  // Yellow
        }
        
        // Draw instruction to restart
        drawText(boxX + 150, boxY + 60, "Press 'R' to restart simulation", 0.9f, 0.9f, 0.9f);
        drawText(boxX + 200, boxY + 30, "or close the window", 0.7f, 0.7f, 0.7f);
    }
}

/**
 * @brief Draw a filled square
 */
void Simulation::drawSquare(float x, float y, float size, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + size, y);
    glVertex2f(x + size, y + size);
    glVertex2f(x, y + size);
    glEnd();
}

/**
 * @brief Draw a filled triangle (centered at x, y)
 */
void Simulation::drawTriangle(float x, float y, float size, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y - size / 2);           // Top vertex
    glVertex2f(x - size / 2, y + size / 2); // Bottom left
    glVertex2f(x + size / 2, y + size / 2); // Bottom right
    glEnd();
}

/**
 * @brief Draw a filled rectangle
 */
void Simulation::drawRectangle(float x, float y, float width, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

/**
 * @brief Draw text at position
 */
void Simulation::drawText(float x, float y, const std::string& text, float r, float g, float b) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }
}

/**
 * @brief Draw a character in the center of a square
 */
void Simulation::drawCharInSquare(float x, float y, char c, float r, float g, float b) {
    glColor3f(r, g, b);
    glRasterPos2f(x - 4, y + 4);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}

/**
 * @brief Draw a line between two points
 */
void Simulation::drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float lineWidth) {
    glColor3f(r, g, b);
    glLineWidth(lineWidth);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
    glLineWidth(1.0f);  // Reset to default
}

/**
 * @brief Render visual effects (shooting lines, explosions)
 */
void Simulation::renderEffects() {
    // Update and render shoot effects
    auto it = shootEffects.begin();
    while (it != shootEffects.end()) {
        // Convert grid positions to screen coordinates (center of cells)
        float x1 = it->from.x * CELL_SIZE + CELL_SIZE / 2;
        float y1 = it->from.y * CELL_SIZE + CELL_SIZE / 2;
        float x2 = it->to.x * CELL_SIZE + CELL_SIZE / 2;
        float y2 = it->to.y * CELL_SIZE + CELL_SIZE / 2;
        
        // Calculate animation progress (0.0 = just fired, 1.0 = fading out)
        float progress = 1.0f - (float)it->turnsRemaining / (float)it->maxTurns;
        float fadeAlpha = (float)it->turnsRemaining / (float)it->maxTurns;  // Fade out over time
        
        if (it->isGrenade) {
            // === GRENADE EFFECT - Improved arc and explosion ===
            
            // Draw grenade arc (parabolic trajectory)
            float grenadeProgress = std::min(1.0f, progress * 1.5f);  // Grenade travels in first 66%
            if (grenadeProgress < 1.0f) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Draw arc with multiple segments
                glLineWidth(3.0f);
                glColor4f(0.3f, 0.3f, 0.3f, fadeAlpha * 0.8f);  // Dark gray grenade
                glBegin(GL_LINE_STRIP);
                for (int i = 0; i <= 20; ++i) {
                    float t = (float)i / 20.0f * grenadeProgress;
                    float arcX = x1 + (x2 - x1) * t;
                    float arcY = y1 + (y2 - y1) * t;
                    // Add parabolic height
                    float arcHeight = 40.0f * sin(t * 3.14159f);
                    arcY += arcHeight;
                    glVertex2f(arcX, arcY);
                }
                glEnd();
                
                // Draw grenade projectile
                float grenadeX = x1 + (x2 - x1) * grenadeProgress;
                float grenadeY = y1 + (y2 - y1) * grenadeProgress + 40.0f * sin(grenadeProgress * 3.14159f);
                glColor4f(0.2f, 0.2f, 0.2f, fadeAlpha);
                float grenadeSize = 5.0f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(grenadeX, grenadeY);
                for (int i = 0; i <= 8; ++i) {
                    float angle = (float)i / 8.0f * 2.0f * 3.14159f;
                    glVertex2f(grenadeX + cos(angle) * grenadeSize, grenadeY + sin(angle) * grenadeSize);
                }
                glEnd();
                glDisable(GL_BLEND);
            }
            
            // Explosion animation (after grenade lands)
            if (grenadeProgress >= 1.0f) {
                float explosionSize = GRENADE_RADIUS * CELL_SIZE;
                float explosionProgress = (progress - 0.66f) / 0.34f;  // Explosion in last 34%
                
                // Explosion grows rapidly then shrinks
                float explosionScale = 1.0f;
                if (explosionProgress < 0.3f) {
                    explosionScale = explosionProgress / 0.3f * 1.2f;  // Grow to 120%
                } else {
                    explosionScale = 1.2f - ((explosionProgress - 0.3f) / 0.7f) * 1.2f;  // Shrink to 0
                }
                float currentExplosionSize = explosionSize * explosionScale;
                
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for bright explosion
                
                // Inner explosion core (white-hot)
                glColor4f(1.0f, 1.0f, 0.9f, fadeAlpha * 0.9f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x2, y2);
                for (int i = 0; i <= 32; ++i) {
                    float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                    glVertex2f(x2 + cos(angle) * currentExplosionSize * 0.4f, y2 + sin(angle) * currentExplosionSize * 0.4f);
                }
                glEnd();
                
                // Middle explosion layer (orange-yellow)
                glColor4f(1.0f, 0.7f, 0.2f, fadeAlpha * 0.7f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x2, y2);
                for (int i = 0; i <= 32; ++i) {
                    float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                    glVertex2f(x2 + cos(angle) * currentExplosionSize * 0.7f, y2 + sin(angle) * currentExplosionSize * 0.7f);
                }
                glEnd();
                
                // Outer explosion layer (red-orange)
                glColor4f(1.0f, 0.3f, 0.1f, fadeAlpha * 0.5f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x2, y2);
                for (int i = 0; i <= 32; ++i) {
                    float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                    glVertex2f(x2 + cos(angle) * currentExplosionSize, y2 + sin(angle) * currentExplosionSize);
                }
                glEnd();
                
                // Shockwave ring
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glLineWidth(4.0f);
                glColor4f(1.0f, 0.8f, 0.3f, fadeAlpha * 0.8f);
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; ++i) {
                    float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                    float shockwaveSize = currentExplosionSize * 1.1f;
                    glVertex2f(x2 + cos(angle) * shockwaveSize, y2 + sin(angle) * shockwaveSize);
                }
                glEnd();
                
                glDisable(GL_BLEND);
            }
        } else {
            // === GUN SHOT EFFECT - Improved tracer and impact ===
            
            // Bullet travels quickly in first 40% of duration
            float bulletProgress = std::min(1.0f, progress * 2.5f);
            float bulletX = x1 + (x2 - x1) * bulletProgress;
            float bulletY = y1 + (y2 - y1) * bulletProgress;
            
            // Team colors for bullets
            float r = (it->team == Team::BLUE) ? 0.4f : 1.0f;
            float g = (it->team == Team::BLUE) ? 0.7f : 0.8f;
            float b = (it->team == Team::BLUE) ? 1.0f : 0.2f;
            
            // === MUZZLE FLASH (first 20% of animation) ===
            if (progress < 0.2f) {
                float flashAlpha = (1.0f - progress / 0.2f) * 0.9f;
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive for bright flash
                
                // Bright white-yellow flash
                glColor4f(1.0f, 1.0f, 0.8f, flashAlpha);
                float flashSize = 12.0f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x1, y1);
                for (int i = 0; i <= 12; ++i) {
                    float angle = (float)i / 12.0f * 2.0f * 3.14159f;
                    glVertex2f(x1 + cos(angle) * flashSize, y1 + sin(angle) * flashSize);
                }
                glEnd();
                
                // Outer orange flash
                glColor4f(1.0f, 0.7f, 0.3f, flashAlpha * 0.6f);
                float outerFlashSize = 18.0f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x1, y1);
                for (int i = 0; i <= 12; ++i) {
                    float angle = (float)i / 12.0f * 2.0f * 3.14159f;
                    glVertex2f(x1 + cos(angle) * outerFlashSize, y1 + sin(angle) * outerFlashSize);
                }
                glEnd();
                
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_BLEND);
            }
            
            // === BULLET TRACER (while traveling) ===
            if (bulletProgress < 1.0f) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bright tracer line (full path)
                glLineWidth(2.5f);
                glColor4f(r, g, b, 0.3f);
                glBegin(GL_LINES);
                glVertex2f(x1, y1);
                glVertex2f(bulletX, bulletY);
                glEnd();
                
                // Bullet trail (intense near bullet head)
                glLineWidth(4.0f);
                float trailLength = 20.0f;
                float dx = x2 - x1;
                float dy = y2 - y1;
                float dist = sqrt(dx * dx + dy * dy);
                if (dist > 0.1f) {
                    dx /= dist;
                    dy /= dist;
                }
                float trailX = bulletX - dx * trailLength;
                float trailY = bulletY - dy * trailLength;
                
                // Gradient trail (bright at head, fading back)
                glBegin(GL_LINES);
                glColor4f(1.0f, 1.0f, 0.9f, 0.9f);  // Bright near bullet
                glVertex2f(bulletX, bulletY);
                glColor4f(r, g, b, 0.2f);  // Fade to team color
                glVertex2f(trailX, trailY);
                glEnd();
                
                // Bullet head (glowing core)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive for glow
                glColor4f(1.0f, 1.0f, 0.95f, 1.0f);
                float bulletSize = 5.0f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(bulletX, bulletY);
                for (int i = 0; i <= 8; ++i) {
                    float angle = (float)i / 8.0f * 2.0f * 3.14159f;
                    glVertex2f(bulletX + cos(angle) * bulletSize, bulletY + sin(angle) * bulletSize);
                }
                glEnd();
                
                // Outer glow
                glColor4f(r, g, b, 0.6f);
                float glowSize = 8.0f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(bulletX, bulletY);
                for (int i = 0; i <= 8; ++i) {
                    float angle = (float)i / 8.0f * 2.0f * 3.14159f;
                    glVertex2f(bulletX + cos(angle) * glowSize, bulletY + sin(angle) * glowSize);
                }
                glEnd();
                
                glDisable(GL_BLEND);
            }
            
            // === IMPACT EFFECT (after bullet arrives) ===
            if (bulletProgress >= 1.0f) {
                float impactProgress = (progress - 0.4f) / 0.6f;  // Impact in last 60%
                float impactAlpha = fadeAlpha * (1.0f - impactProgress * 0.5f);
                
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive for bright flash
                
                // Bright impact flash (grows then fades)
                float impactSize = 8.0f * (1.0f + impactProgress * 0.8f);
                glColor4f(1.0f, 1.0f, 0.7f, impactAlpha * 0.9f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x2, y2);
                for (int i = 0; i <= 12; ++i) {
                    float angle = (float)i / 12.0f * 2.0f * 3.14159f;
                    glVertex2f(x2 + cos(angle) * impactSize, y2 + sin(angle) * impactSize);
                }
                glEnd();
                
                // Orange impact glow
                glColor4f(1.0f, 0.6f, 0.2f, impactAlpha * 0.6f);
                float impactGlow = impactSize * 1.5f;
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x2, y2);
                for (int i = 0; i <= 12; ++i) {
                    float angle = (float)i / 12.0f * 2.0f * 3.14159f;
                    glVertex2f(x2 + cos(angle) * impactGlow, y2 + sin(angle) * impactGlow);
                }
                glEnd();
                
                // Impact sparks (radial lines)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glLineWidth(2.5f);
                glColor4f(1.0f, 0.8f, 0.3f, impactAlpha * 0.8f);
                glBegin(GL_LINES);
                for (int i = 0; i < 8; ++i) {
                    float angle = (float)i / 8.0f * 2.0f * 3.14159f;
                    float sparkLength = 12.0f * (1.0f - impactProgress * 0.5f);
                    glVertex2f(x2, y2);
                    glVertex2f(x2 + cos(angle) * sparkLength, y2 + sin(angle) * sparkLength);
                }
                glEnd();
                
                glDisable(GL_BLEND);
            }
        }
        
        // Decrement turns remaining
        it->turnsRemaining--;
        if (it->turnsRemaining <= 0) {
            it = shootEffects.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Render order lines from commanders to warriors
 */
void Simulation::renderOrderLines() {
    for (Character* c : allCharacters) {
        if (!c->isAlive() || c->getType() == CharacterType::COMMANDER) {
            continue;
        }
        
        // FIX E: Hide order lines for warriors in autonomous mode
        // When warriors act independently (per assignment: "perform actions...in way independent")
        // they ignore commander orders, so showing order line would be misleading
        if (c->getType() == CharacterType::WARRIOR) {
            Warrior* warrior = dynamic_cast<Warrior*>(c);
            if (warrior && warrior->isInAutonomousMode()) {
                continue;  // Skip - warrior acting independently, no order line needed
            }
        }
        
        // Get character's current order
        Order order = c->getCurrentOrder();
        if (order.type == OrderType::NONE) {
            continue;
        }
        
        // Draw line from character to target
        float x1 = c->getPosition().x * CELL_SIZE + CELL_SIZE / 2;
        float y1 = c->getPosition().y * CELL_SIZE + CELL_SIZE / 2;
        float x2 = order.targetPosition.x * CELL_SIZE + CELL_SIZE / 2;
        float y2 = order.targetPosition.y * CELL_SIZE + CELL_SIZE / 2;
        
        // Color based on order type
        float r = 1.0f, g = 1.0f, b = 1.0f;
        
        // FIX B: For ATTACK orders on warriors, show actual combat target if shooting
        if (order.type == OrderType::ATTACK && c->getType() == CharacterType::WARRIOR) {
            Warrior* warrior = dynamic_cast<Warrior*>(c);
            if (warrior && warrior->didShootThisTurn()) {
                // Draw to actual shot target (bright red)
                Position shotTarget = warrior->getLastShotTarget();
                x2 = shotTarget.x * CELL_SIZE + CELL_SIZE / 2;
                y2 = shotTarget.y * CELL_SIZE + CELL_SIZE / 2;
                r = 1.0f; g = 0.0f; b = 0.0f;  // Bright red for active combat
            } else {
                // FIX D: Check if order target is stale (no recent enemy sighting)
                // Find the commander for this character's team
                Commander* commander = nullptr;
                const std::vector<Character*>& team = (c->getTeam() == Team::BLUE) ? blueTeam : orangeTeam;
                for (Character* member : team) {
                    if (member->getType() == CharacterType::COMMANDER) {
                        commander = dynamic_cast<Commander*>(member);
                        break;
                    }
                }
                
                bool isStale = false;
                if (commander) {
                    const auto& enemyMap = commander->getCombinedEnemyMap();
                    auto it = enemyMap.find(order.targetPosition);
                    if (it != enemyMap.end()) {
                        int turnsSinceSeen = currentTurn - it->second.turnSeen;
                        if (turnsSinceSeen >= 3) {
                            isStale = true;
                        }
                    } else {
                        // Target not in enemy map at all - very stale
                        isStale = true;
                    }
                }
                
                if (isStale) {
                    // Yellow for stale orders (no recent enemy at target)
                    r = 1.0f; g = 1.0f; b = 0.0f;
                } else {
                    // Gray for moving to target (not shooting yet, but target is valid)
                    r = 0.5f; g = 0.5f; b = 0.5f;
                }
            }
        } else {
            // Color based on order type (non-warrior or non-attack orders)
            switch (order.type) {
                case OrderType::ATTACK:
                    r = 1.0f; g = 0.0f; b = 0.0f;  // Red for attack
                    break;
                case OrderType::DEFEND:
                    r = 0.0f; g = 1.0f; b = 1.0f;  // Cyan for defend
                    break;
                case OrderType::MOVE:
                    r = 1.0f; g = 0.0f; b = 1.0f;  // Magenta for move (better visibility)
                    break;
                case OrderType::HEAL:
                    r = 0.0f; g = 1.0f; b = 0.0f;  // Green for heal
                    break;
                case OrderType::RESUPPLY:
                    r = 1.0f; g = 0.5f; b = 0.0f;  // Orange for resupply
                    break;
                default:
                    break;
            }
        }
        
        // Draw dotted line (draw short segments with gaps)
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrt(dx * dx + dy * dy);
        float nx = dx / length;  // Normalized direction
        float ny = dy / length;
        
        float segmentLength = 5.0f;
        float gapLength = 3.0f;
        float totalSegment = segmentLength + gapLength;
        
        for (float dist = 0; dist < length; dist += totalSegment) {
            float startDist = dist;
            float endDist = std::min(dist + segmentLength, length);
            drawLine(x1 + nx * startDist, y1 + ny * startDist,
                    x1 + nx * endDist, y1 + ny * endDist,
                    r, g, b, 1.0f);
        }
    }
}

/**
 * @brief Render fog of war - show what each team can see
 */
void Simulation::renderFogOfWar() {
    // Create a semi-transparent overlay for unseen areas
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw fog over entire map first
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            Position pos(x, y);
            bool blueCanSee = false;
            bool orangeCanSee = false;
            
            // Check if any blue team member can see this position
            for (Character* c : blueTeam) {
                if (c->isAlive() && c->canSee(pos)) {
                    blueCanSee = true;
                    break;
                }
            }
            
            // Check if any orange team member can see this position
            for (Character* c : orangeTeam) {
                if (c->isAlive() && c->canSee(pos)) {
                    orangeCanSee = true;
                    break;
                }
            }
            
            float screenX = x * CELL_SIZE;
            float screenY = y * CELL_SIZE;
            
            if (blueCanSee && orangeCanSee) {
                // Both teams can see - highlight in purple
                glColor4f(0.5f, 0.0f, 0.5f, 0.2f);  // Purple tint
            } else if (blueCanSee) {
                // Only blue team can see - blue tint
                glColor4f(0.0f, 0.3f, 0.6f, 0.15f);
            } else if (orangeCanSee) {
                // Only orange team can see - orange tint
                glColor4f(0.6f, 0.3f, 0.0f, 0.15f);
            } else {
                // Neither team can see - dark fog
                glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
            }
            
            // Draw overlay
            glBegin(GL_QUADS);
            glVertex2f(screenX, screenY);
            glVertex2f(screenX + CELL_SIZE, screenY);
            glVertex2f(screenX + CELL_SIZE, screenY + CELL_SIZE);
            glVertex2f(screenX, screenY + CELL_SIZE);
            glEnd();
        }
    }
    
    glDisable(GL_BLEND);
}

/**
 * @brief Render vision cones for all characters
 */
void Simulation::renderVisionCones() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw vision cones for all alive characters
    for (Character* c : allCharacters) {
        if (!c->isAlive()) continue;
        
        Position pos = c->getPosition();
        float screenX = pos.x * CELL_SIZE + CELL_SIZE / 2;
        float screenY = pos.y * CELL_SIZE + CELL_SIZE / 2;
        
        // Use SHOOT_RANGE as vision radius (8 tiles)
        float radius = SHOOT_RANGE * CELL_SIZE;
        
        // Determine facing direction based on team default orientation
        // Blue team (left side) faces right (east)
        // Orange team (right side) faces left (west)
        float facingAngle;
        if (c->getTeam() == Team::BLUE) {
            facingAngle = 0.0f;  // Facing right (0 degrees = east)
        } else {
            facingAngle = 3.14159f;  // Facing left (180 degrees = west)
        }
        
        // Draw cone with 120-degree field of view
        float coneArc = 2.0944f;  // 120 degrees in radians
        float startAngle = facingAngle - coneArc / 2;
        float endAngle = facingAngle + coneArc / 2;
        
        // Team colors with transparency
        if (c->getTeam() == Team::BLUE) {
            glColor4f(0.0f, 0.5f, 1.0f, 0.2f);  // Blue
        } else {
            glColor4f(1.0f, 0.5f, 0.0f, 0.2f);  // Orange
        }
        
        // Draw cone as triangle fan
        int segments = 30;
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(screenX, screenY);  // Center point at character
        
        for (int i = 0; i <= segments; i++) {
            float angle = startAngle + (endAngle - startAngle) * i / segments;
            float dx = radius * cos(angle);
            float dy = radius * sin(angle);
            glVertex2f(screenX + dx, screenY + dy);
        }
        glEnd();
        
        // Draw cone outline
        glLineWidth(1.5f);
        if (c->getTeam() == Team::BLUE) {
            glColor4f(0.0f, 0.5f, 1.0f, 0.5f);
        } else {
            glColor4f(1.0f, 0.5f, 0.0f, 0.5f);
        }
        
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segments; i++) {
            float angle = startAngle + (endAngle - startAngle) * i / segments;
            float dx = radius * cos(angle);
            float dy = radius * sin(angle);
            glVertex2f(screenX + dx, screenY + dy);
        }
        glEnd();
        
        // Draw lines from center to cone edges
        glBegin(GL_LINES);
        float dx1 = radius * cos(startAngle);
        float dy1 = radius * sin(startAngle);
        glVertex2f(screenX, screenY);
        glVertex2f(screenX + dx1, screenY + dy1);
        
        float dx2 = radius * cos(endAngle);
        float dy2 = radius * sin(endAngle);
        glVertex2f(screenX, screenY);
        glVertex2f(screenX + dx2, screenY + dy2);
        glEnd();
    }
    
    glDisable(GL_BLEND);
}

/**
 * @brief Render weapon ranges for warriors
 */
void Simulation::renderWeaponRanges() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Only draw ranges for warriors
    for (Character* c : allCharacters) {
        if (!c->isAlive() || c->getType() != CharacterType::WARRIOR) continue;
        
        Position pos = c->getPosition();
        float screenX = pos.x * CELL_SIZE + CELL_SIZE / 2;
        float screenY = pos.y * CELL_SIZE + CELL_SIZE / 2;
        
        float radius = SHOOT_RANGE * CELL_SIZE;
        
        // Draw range circle (outline only)
        if (c->getTeam() == Team::BLUE) {
            drawCircle(screenX, screenY, radius, 0.0f, 0.7f, 1.0f, 0.5f, false);  // Blue outline
        } else {
            drawCircle(screenX, screenY, radius, 1.0f, 0.6f, 0.0f, 0.5f, false);  // Orange outline
        }
    }
    
    glDisable(GL_BLEND);
}

/**
 * @brief Draw a circle (filled or outline)
 */
void Simulation::drawCircle(float x, float y, float radius, float r, float g, float b, float alpha, bool filled) {
    glColor4f(r, g, b, alpha);
    
    int segments = 50;
    
    if (filled) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);  // Center point
        for (int i = 0; i <= segments; i++) {
            float angle = 2.0f * 3.14159f * i / segments;
            float dx = radius * cos(angle);
            float dy = radius * sin(angle);
            glVertex2f(x + dx, y + dy);
        }
        glEnd();
    } else {
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; i++) {
            float angle = 2.0f * 3.14159f * i / segments;
            float dx = radius * cos(angle);
            float dy = radius * sin(angle);
            glVertex2f(x + dx, y + dy);
        }
        glEnd();
    }
}
