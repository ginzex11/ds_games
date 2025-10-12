#include "simulation.h"
#include <sstream>
#include <iomanip>

/**
 * @brief Construct simulation and initialize game
 */
Simulation::Simulation()
    : currentTurn(0), gameOver(false), winner(Team::BLUE),
      paused(false), turnDelay(0.5f), timeSinceLastTurn(0.0f), showFogOfWar(false) {
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
    int blueAlive = countAliveCharacters(Team::BLUE);
    int orangeAlive = countAliveCharacters(Team::ORANGE);
    
    if (blueAlive == 0) {
        gameOver = true;
        winner = Team::ORANGE;
    } else if (orangeAlive == 0) {
        gameOver = true;
        winner = Team::BLUE;
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
        case CellType::WAREHOUSE:
            r = 0.9f; g = 0.9f; b = 0.3f;  // Yellow
            drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
            break;
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
        std::string ammoText = std::to_string(ammo) + "/50";
        
        // Draw dark background for better text visibility
        drawRectangle(screenX + 1, textYPos - 2, 22, 10, 0.0f, 0.0f, 0.0f);
        
        // Draw ammo in bright cyan (excellent visibility)
        drawText(screenX + 2, textYPos, ammoText.c_str(), 0.0f, 1.0f, 1.0f);
    } else if (character->getType() == CharacterType::MEDIC) {
        Medic* medic = static_cast<Medic*>(character);
        int medicine = medic->getMedicineSupplies();
        std::string medText = "M:" + std::to_string(medicine);
        
        // Draw dark background
        drawRectangle(screenX + 1, textYPos - 2, 18, 10, 0.0f, 0.0f, 0.0f);
        
        // Draw medicine in bright green
        drawText(screenX + 2, textYPos, medText.c_str(), 0.0f, 1.0f, 0.0f);
    } else if (character->getType() == CharacterType::SUPPLIER) {
        Supplier* supplier = static_cast<Supplier*>(character);
        int supplies = supplier->getAmmoSupplies();
        std::string supText = "A:" + std::to_string(supplies);
        
        // Draw dark background
        drawRectangle(screenX + 1, textYPos - 2, 18, 10, 0.0f, 0.0f, 0.0f);
        
        // Draw ammo supplies in bright yellow
        drawText(screenX + 2, textYPos, supText.c_str(), 1.0f, 1.0f, 0.0f);
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
            // Grenade - draw arc (approximated with line for now) in red
            drawLine(x1, y1, x2, y2, 1.0f, 0.3f, 0.0f, 3.0f);
            
            // Draw explosion circle at target
            float explosionSize = 8.0f;
            glColor3f(1.0f, 0.5f, 0.0f);  // Orange explosion
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x2, y2);
            for (int i = 0; i <= 16; ++i) {
                float angle = (float)i / 16.0f * 2.0f * 3.14159f;
                glVertex2f(x2 + cos(angle) * explosionSize, y2 + sin(angle) * explosionSize);
            }
            glEnd();
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
