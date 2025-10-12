#ifndef SIMULATION_H
#define SIMULATION_H

#include "common.h"
#include "map.h"
#include "character.h"
#include "commander.h"
#include "warrior.h"
#include "medic.h"
#include "supplier.h"
#include <GL/freeglut.h>

/**
 * @brief Simulation class - manages game state and rendering
 * 
 * Responsibilities:
 * - Initialize teams and map
 * - Update all characters each turn
 * - Handle rendering with OpenGL
 * - Track game state and victory conditions
 * - Provide user interface controls
 */
class Simulation {
private:
    Map gameMap;
    std::vector<Character*> allCharacters;
    std::vector<Character*> blueTeam;
    std::vector<Character*> orangeTeam;
    int currentTurn;
    bool gameOver;
    Team winner;
    bool paused;
    float turnDelay;
    float timeSinceLastTurn;
    
    // Rendering helpers
    void renderGrid();
    void renderCell(int x, int y);
    void renderCharacter(Character* character);
    void renderUI();
    void drawSquare(float x, float y, float size, float r, float g, float b);
    void drawText(float x, float y, const std::string& text, float r = 1.0f, float g = 1.0f, float b = 1.0f);
    void drawCharInSquare(float x, float y, char c, float r = 1.0f, float g = 1.0f, float b = 1.0f);
    
    // Game logic helpers
    void initializeTeams();
    void updateAllCharacters();
    void checkVictoryConditions();
    Team getAliveTeam();
    int countAliveCharacters(Team team);
    
public:
    Simulation();
    ~Simulation();
    
    // Main loop functions
    void update(float deltaTime);
    void render();
    
    // Control functions
    void togglePause() { paused = !paused; }
    void reset();
    void speedUp() { turnDelay = std::max(0.1f, turnDelay - 0.1f); }
    void slowDown() { turnDelay = std::min(2.0f, turnDelay + 0.1f); }
    
    // Getters
    bool isGameOver() const { return gameOver; }
    bool isPaused() const { return paused; }
    int getCurrentTurn() const { return currentTurn; }
    Team getWinner() const { return winner; }
    const Map& getMap() const { return gameMap; }
};

#endif // SIMULATION_H
