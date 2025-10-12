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
#include <vector>
#include <deque>

/**
 * @brief Visual effect for shooting
 */
struct ShootEffect {
    Position from;
    Position to;
    bool isGrenade;
    int turnsRemaining;
    Team team;
    
    ShootEffect(Position f, Position t, bool grenade, Team tm) 
        : from(f), to(t), isGrenade(grenade), turnsRemaining(2), team(tm) {}
};

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
    bool showFogOfWar;  // Toggle for visibility visualization
    bool showVisionCones;  // Toggle for vision cone visualization
    bool showWeaponRanges;  // Toggle for weapon range circles
    
    // Visual effects
    std::deque<ShootEffect> shootEffects;
    
    // Rendering helpers
    void renderGrid();
    void renderCell(int x, int y);
    void renderCharacter(Character* character);
    void renderUI();
    void renderEffects();
    void renderOrderLines();
    void renderFogOfWar();
    void renderVisionCones();
    void renderWeaponRanges();
    void drawCircle(float x, float y, float radius, float r, float g, float b, float alpha = 1.0f, bool filled = false);
    void drawSquare(float x, float y, float size, float r, float g, float b);
    void drawTriangle(float x, float y, float size, float r, float g, float b);
    void drawRectangle(float x, float y, float width, float height, float r, float g, float b);
    void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float lineWidth = 2.0f);
    void drawText(float x, float y, const std::string& text, float r = 1.0f, float g = 1.0f, float b = 1.0f);
    void drawCharInSquare(float x, float y, char c, float r = 1.0f, float g = 1.0f, float b = 1.0f);
    
    // Game logic helpers
    void initializeTeams();
    void updateAllCharacters();
    void checkVictoryConditions();
    Team getAliveTeam();
    int countAliveCharacters(Team team);
    bool isPositionOccupied(const Position& pos, const Character* excludeChar = nullptr) const;
    
public:
    Simulation();
    ~Simulation();
    
    // Main loop functions
    void update(float deltaTime);
    void render();
    
    // Control functions
    void togglePause() { paused = !paused; }
    void toggleFogOfWar() { showFogOfWar = !showFogOfWar; }
    void toggleVisionCones() { showVisionCones = !showVisionCones; }
    void toggleWeaponRanges() { showWeaponRanges = !showWeaponRanges; }
    void reset();
    void speedUp() { turnDelay = std::max(0.1f, turnDelay - 0.1f); }
    void slowDown() { turnDelay = std::min(2.0f, turnDelay + 0.1f); }
    
    // Visual effects
    void addShootEffect(Position from, Position to, bool isGrenade, Team team) {
        shootEffects.push_back(ShootEffect(from, to, isGrenade, team));
    }
    
    // Getters
    bool isGameOver() const { return gameOver; }
    bool isPaused() const { return paused; }
    int getCurrentTurn() const { return currentTurn; }
    Team getWinner() const { return winner; }
    const Map& getMap() const { return gameMap; }
};

#endif // SIMULATION_H
