#include "simulation.h"
#include <sstream>
#include <iomanip>

/**
 * @brief Construct simulation and initialize game
 */
Simulation::Simulation()
    : currentTurn(0), gameOver(false), winner(Team::BLUE),
      paused(false), turnDelay(0.5f), timeSinceLastTurn(0.0f) {
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
 * @brief Render the entire simulation
 */
void Simulation::render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    
    renderGrid();
    
    // Render all characters
    for (Character* c : allCharacters) {
        if (c->isAlive()) {
            renderCharacter(c);
        }
    }
    
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
    float r = 0.9f, g = 0.9f, b = 0.9f;  // Default: light gray (empty)
    
    switch (cell.type) {
        case CellType::ROCK:
            r = 0.3f; g = 0.3f; b = 0.3f;  // Dark gray
            break;
        case CellType::TREE:
            r = 0.2f; g = 0.6f; b = 0.2f;  // Green
            break;
        case CellType::WATER:
            r = 0.3f; g = 0.5f; b = 0.8f;  // Blue
            break;
        case CellType::WAREHOUSE:
            r = 0.9f; g = 0.9f; b = 0.3f;  // Yellow
            break;
        default:
            break;
    }
    
    drawSquare(screenX, screenY, CELL_SIZE - 1, r, g, b);
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
    
    // Draw health bar BELOW the character square (smaller and separate)
    float healthPercent = character->getHealth() / 100.0f;
    float barWidth = (CELL_SIZE - 8) * healthPercent;
    float barHeight = 3.0f;  // Thin bar
    
    // Black background for health bar
    drawSquare(screenX + 4, screenY - 6, CELL_SIZE - 8, 0.0f, 0.0f, 0.0f);
    
    // Green health bar on top
    if (healthPercent > 0.5f) {
        drawSquare(screenX + 4, screenY - 6, barWidth, 0.0f, 1.0f, 0.0f);  // Green
    } else if (healthPercent > 0.25f) {
        drawSquare(screenX + 4, screenY - 6, barWidth, 1.0f, 1.0f, 0.0f);  // Yellow
    } else {
        drawSquare(screenX + 4, screenY - 6, barWidth, 1.0f, 0.0f, 0.0f);  // Red
    }
}

/**
 * @brief Render UI overlay
 */
void Simulation::renderUI() {
    // Draw info panel at top
    std::ostringstream oss;
    oss << "Turn: " << currentTurn 
        << " | Blue: " << countAliveCharacters(Team::BLUE)
        << " | Orange: " << countAliveCharacters(Team::ORANGE);
    
    if (paused) {
        oss << " | PAUSED";
    }
    
    if (gameOver) {
        oss << " | GAME OVER - " << teamToString(winner) << " WINS!";
    }
    
    drawText(10, WINDOW_HEIGHT - 20, oss.str());
    
    // Draw controls with high visibility
    drawText(10, 20, "Controls: SPACE=Pause | R=Reset | +/- Speed", 1.0f, 1.0f, 0.0f);  // Bright yellow
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
