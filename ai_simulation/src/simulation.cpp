#include "simulation.h"
#include <sstream>
#include <iomanip>

/**
 * @brief Construct simulation and initialize game
 */
Simulation::Simulation()
    : currentTurn(0), gameOver(false), winner(Team::BLUE),
      paused(false), turnDelay(0.5f), timeSinceLastTurn(0.0f), showFogOfWar(true),
      showVisionCones(false), showWeaponRanges(false) {
    initializeTeams();
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
    if (paused || gameOver) return;
    
    timeSinceLastTurn += deltaTime;
    
    if (timeSinceLastTurn >= turnDelay) {
        timeSinceLastTurn = 0.0f;
        currentTurn++;
        
        updateAllCharacters();
        checkVictoryConditions();
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
    
    for (Character* c : allCharacters) {
        if (c->isAlive() && c->getType() == CharacterType::WARRIOR) {
            if (c->getTeam() == Team::BLUE) {
                blueWarriors++;
            } else {
                orangeWarriors++;
            }
        }
    }
    
    if (blueWarriors == 0 && orangeWarriors > 0) {
        gameOver = true;
        winner = Team::ORANGE;
        LOG_CONTROL("=== GAME OVER: Orange team wins! ===\n");
    } else if (orangeWarriors == 0 && blueWarriors > 0) {
        gameOver = true;
        winner = Team::BLUE;
        LOG_CONTROL("=== GAME OVER: Blue team wins! ===\n");
    } else if (blueWarriors == 0 && orangeWarriors == 0) {
        gameOver = true;
        winner = Team::BLUE;  // Draw defaults to Blue
        LOG_CONTROL("=== GAME OVER: Draw! All warriors eliminated. ===\n");
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
            glColor3f(1.0f, 1.0f, 1.0f);  // White text
            glRasterPos2f(screenX + CELL_SIZE/3, screenY + CELL_SIZE/2);
            
            if (warehouseType == WarehouseType::MEDICINE) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'M');
            } else if (warehouseType == WarehouseType::AMMO) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'A');
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
    float healthPercent = character->getHealth() / 100.0f;
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
        oss1 << " | GAME OVER - " << teamToString(winner) << " WINS!";
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
        
        // Victory popup box (centered)
        float boxWidth = 500.0f;
        float boxHeight = 200.0f;
        float boxX = (WINDOW_WIDTH - boxWidth) / 2.0f;
        float boxY = (WINDOW_HEIGHT - boxHeight) / 2.0f;
        
        // Draw popup box background (team color)
        if (winner == Team::BLUE) {
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
        
        // Draw "VICTORY!" text (large, centered)
        std::string victoryText = "VICTORY!";
        drawText(boxX + 150, boxY + boxHeight - 50, victoryText.c_str(), 1.0f, 1.0f, 1.0f);
        
        // Draw winner text
        std::ostringstream winnerText;
        winnerText << teamToString(winner) << " Team Wins!";
        drawText(boxX + 130, boxY + boxHeight - 90, winnerText.str().c_str(), 1.0f, 1.0f, 1.0f);
        
        // Draw instruction to restart
        drawText(boxX + 100, boxY + 60, "Press 'R' to restart simulation", 0.9f, 0.9f, 0.9f);
        drawText(boxX + 140, boxY + 30, "or close the window", 0.7f, 0.7f, 0.7f);
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
        
        if (it->isGrenade) {
            // Grenade - draw arc (approximated with line for now) in red/orange
            glLineWidth(4.0f);
            glColor3f(1.0f, 0.0f, 0.0f);  // Bright red for grenade throw
            glBegin(GL_LINES);
            glVertex2f(x1, y1);
            glVertex2f(x2, y2);
            glEnd();
            
            // Draw LARGE explosion circle at target with radius matching GRENADE_RADIUS
            float explosionSize = GRENADE_RADIUS * CELL_SIZE;  // Match game radius (2 tiles)
            
            // Draw filled explosion
            glColor4f(1.0f, 0.3f, 0.0f, 0.7f);  // Bright orange with alpha
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x2, y2);
            for (int i = 0; i <= 32; ++i) {
                float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                glVertex2f(x2 + cos(angle) * explosionSize, y2 + sin(angle) * explosionSize);
            }
            glEnd();
            
            // Draw explosion outline
            glLineWidth(3.0f);
            glColor3f(1.0f, 0.0f, 0.0f);  // Red outline
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 32; ++i) {
                float angle = (float)i / 32.0f * 2.0f * 3.14159f;
                glVertex2f(x2 + cos(angle) * explosionSize, y2 + sin(angle) * explosionSize);
            }
            glEnd();
            
            // Draw "GRENADE!" text indicator
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow text
        } else {
            // Gun shot - draw line in team color
            float r = (it->team == Team::BLUE) ? 0.3f : 1.0f;
            float g = (it->team == Team::BLUE) ? 0.5f : 0.6f;
            float b = (it->team == Team::BLUE) ? 1.0f : 0.2f;
            drawLine(x1, y1, x2, y2, r, g, b, 2.0f);
            
            // Draw impact marker at target
            float impactSize = 4.0f;
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow impact
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x2, y2);
            for (int i = 0; i <= 8; ++i) {
                float angle = (float)i / 8.0f * 2.0f * 3.14159f;
                glVertex2f(x2 + cos(angle) * impactSize, y2 + sin(angle) * impactSize);
            }
            glEnd();
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
