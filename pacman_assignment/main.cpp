#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <queue>
#include <stack>
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>
#include <windows.h>

#include <GL/freeglut.h>
#include <GL/glut.h>

// #include "glew.h"  // Commented out to avoid compilation issues

#include "Cell.h"
#include "Node.h"
#include "CompareNodes.h"

using namespace std;

const int WIDTH = 600;
const int HEIGHT = 600;

const int MSZ = 35; // Smaller maze for visibility

const int WALL = 1;
const int SPACE = 0;
const int PACMAN = 2;
const int GHOST1 = 3;
const int GHOST2 = 4;
const int GHOST3 = 5;
const int COIN = 6;

enum GameState { MENU, PLAYING, GAME_OVER, WIN };
GameState currentState = MENU;

int maze[MSZ][MSZ];

struct Position {
    int row, col;
    Position(int r = 0, int c = 0) : row(r), col(c) {}
    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

Position pacmanPos;
Position ghostPos[3];
vector<Position> coins;
int initialCoins;

int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; // up, down, left, right

int lastPacDir = -1; // Track Pac-Man's last direction for ghost prediction

// Forward declarations
void movePacman();
void moveGhosts();
bool checkCollision();
void initMaze();
void resetGame();
void drawText(float x, float y, const char* text);
void display();
void idle();
void mouse(int button, int state, int x, int y);
void placeTShape(int i, int j);
void placeHShape(int i, int j);
void placeSShape(int i, int j);

double heuristic(Position a, Position b) {
    return abs(a.row - b.row) + abs(a.col - b.col); // Manhattan
}

// Predict Pac-Man's next position based on last direction
Position predictPacman(Position pacPos, int lastDir) {
    Position pred = pacPos;
    if (lastDir == 0) pred.row--; // UP
    else if (lastDir == 1) pred.row++; // DOWN
    else if (lastDir == 2) pred.col--; // LEFT
    else if (lastDir == 3) pred.col++; // RIGHT
    if (pred.row >= 0 && pred.row < MSZ && pred.col >= 0 && pred.col < MSZ && maze[pred.row][pred.col] != WALL) {
        return pred;
    }
    return pacPos; // Fallback
}

// Idle function for game loop
void idle() {
    static int frameCount = 0;
    frameCount++;
    if (currentState == PLAYING) {
        if (frameCount % 2 == 0) { // Move Pac-Man every 4 frames for consistent speed
            movePacman();
        }
        if (frameCount % 3 == 0) { // Move ghosts every 8 frames (slower than Pac-Man)
            moveGhosts();
        }
        if (checkCollision()) {
            cout << "Game Over! Final score: " << (initialCoins - (int)coins.size()) << endl;
            currentState = GAME_OVER;
        }
        if (coins.empty()) {
            cout << "You Win! Final score: " << initialCoins << endl;
            currentState = WIN;
        }
        glutPostRedisplay();
#ifdef _WIN32
        Sleep(100); // Faster frame rate
#else
        usleep(100000);
#endif
    }
}

// A* for ghosts
vector<Position> aStar(Position start, Position goal) {
    // Implement A* using Node and priority_queue
    priority_queue<Node*, vector<Node*>, CompareNodes> openList;
    vector<vector<bool>> closed(MSZ, vector<bool>(MSZ, false));
    vector<vector<Node*>> nodes(MSZ, vector<Node*>(MSZ, nullptr));

    Node* startNode = new Node(new Cell(start.row, start.col, nullptr), 0, heuristic(start, goal));
    openList.push(startNode);
    nodes[start.row][start.col] = startNode;

    int steps = 0; // Prevent infinite loop
    while (!openList.empty() && steps < MSZ * MSZ) {
        steps++;
        Node* current = openList.top();
        openList.pop();
        Position currPos(current->getCell()->getRow(), current->getCell()->getCol());

        if (currPos == goal) {
            // Reconstruct path
            vector<Position> path;
            while (current != nullptr) {
                path.push_back(Position(current->getCell()->getRow(), current->getCell()->getCol()));
                current = current->getParent();
            }
            reverse(path.begin(), path.end());
            
            // Clean up memory
            for (auto& row : nodes) {
                for (auto& n : row) {
                    if (n) {
                        delete n->getCell();
                        delete n;
                    }
                }
            }
            return path;
        }

        closed[currPos.row][currPos.col] = true;

        for (auto& dir : directions) {
            int nr = currPos.row + dir[0];
            int nc = currPos.col + dir[1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL && !closed[nr][nc]) {
                double g = current->getG() + 1;
                double h = heuristic(Position(nr, nc), goal);
                Node* neighbor = new Node(new Cell(nr, nc, nullptr), g, h, current);
                
                if (nodes[nr][nc] == nullptr || g + h < nodes[nr][nc]->getF()) {
                    if (nodes[nr][nc]) {
                        delete nodes[nr][nc]->getCell();
                        delete nodes[nr][nc];
                    }
                    nodes[nr][nc] = neighbor;
                    openList.push(neighbor);
                } else {
                    delete neighbor->getCell();
                    delete neighbor;
                }
            }
        }
    }
    
    // Clean up memory if no path found
    for (auto& row : nodes) {
        for (auto& n : row) {
            if (n) {
                delete n->getCell();
                delete n;
            }
        }
    }
    return {};
}

// BFS limited depth for Pac-Man - returns the next move direction
int bfsLimitedForEscape(Position start, int maxDepth) {
    queue<pair<Position, int>> q;
    vector<vector<bool>> visited(MSZ, vector<bool>(MSZ, false));
    q.push({start, 0});
    visited[start.row][start.col] = true;

    // Find closest ghost first
    Position closestGhost = ghostPos[0];
    double minDist = heuristic(start, ghostPos[0]);
    for (int i = 1; i < 3; i++) {
        double dist = heuristic(start, ghostPos[i]);
        if (dist < minDist) {
            minDist = dist;
            closestGhost = ghostPos[i];
        }
    }

    // If ghost is too close (distance <= 3), try to move away
    if (minDist <= 3) {
        int bestDir = -1;
        double maxDist = 0;
        
        for (int dir = 0; dir < 4; dir++) {
            int nr = start.row + directions[dir][0];
            int nc = start.col + directions[dir][1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
                double dist = heuristic(Position(nr, nc), closestGhost);
                if (dist > maxDist) {
                    maxDist = dist;
                    bestDir = dir;
                }
            }
        }
        return bestDir;
    }
    
    return -1; // No immediate threat
}

// Find nearest coin
Position nearestCoin(Position start) {
    Position nearest = {-1, -1};
    double minDist = 1e9;
    for (auto& coin : coins) {
        double dist = heuristic(coin, start);
        if (dist < minDist) {
            minDist = dist;
            nearest = coin;
        }
    }
    return nearest;
}

// Get direction to move towards target
int getDirectionToTarget(Position from, Position to) {
    int dr = to.row - from.row;
    int dc = to.col - from.col;
    
    // Choose direction based on larger distance
    if (abs(dr) >= abs(dc)) {
        if (dr > 0) return 1; // DOWN
        if (dr < 0) return 0; // UP
    } else {
        if (dc > 0) return 3; // RIGHT
        if (dc < 0) return 2; // LEFT
    }
    return -1;
}

// Move Pac-Man with improved logic
void movePacman() {
    int escapeDir = bfsLimitedForEscape(pacmanPos, 5);
    int chosenDir = -1;
    
    if (escapeDir != -1) {
        // Ghost is close, try to escape
        chosenDir = escapeDir;
    } else {
        // No immediate threat, go for nearest coin
        Position target = nearestCoin(pacmanPos);
        if (target.row != -1) {
            chosenDir = getDirectionToTarget(pacmanPos, target);
        }
    }
    
    // Try the chosen direction
    if (chosenDir != -1) {
        int nr = pacmanPos.row + directions[chosenDir][0];
        int nc = pacmanPos.col + directions[chosenDir][1];
        
        if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
            pacmanPos = Position(nr, nc);
            lastPacDir = chosenDir;
            
            // Check coin collection
            auto it = find(coins.begin(), coins.end(), pacmanPos);
            if (it != coins.end()) {
                coins.erase(it);
            }
            return;
        }
    }
    
    // If chosen direction is blocked, try any available direction
    // Prioritize directions that keep distance from ghosts
    int bestDir = -1;
    double maxMinGhostDist = -1;
    
    for (int dir = 0; dir < 4; dir++) {
        int nr = pacmanPos.row + directions[dir][0];
        int nc = pacmanPos.col + directions[dir][1];
        
        if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
            // Calculate minimum distance to any ghost from this position
            double minGhostDist = 1e9;
            for (int i = 0; i < 3; i++) {
                double dist = heuristic(Position(nr, nc), ghostPos[i]);
                if (dist < minGhostDist) {
                    minGhostDist = dist;
                }
            }
            
            if (minGhostDist > maxMinGhostDist) {
                maxMinGhostDist = minGhostDist;
                bestDir = dir;
            }
        }
    }
    
    // Move in the safest available direction
    if (bestDir != -1) {
        int nr = pacmanPos.row + directions[bestDir][0];
        int nc = pacmanPos.col + directions[bestDir][1];
        pacmanPos = Position(nr, nc);
        lastPacDir = bestDir;
        
        // Check coin collection
        auto it = find(coins.begin(), coins.end(), pacmanPos);
        if (it != coins.end()) {
            coins.erase(it);
        }
    }
}

// Move ghosts
void moveGhosts() {
    for (int i = 0; i < 3; i++) {
        Position goal = pacmanPos;
        if (i == 0) { // First ghost predicts Pac-Man's position
            goal = predictPacman(pacmanPos, lastPacDir);
        }
        vector<Position> path = aStar(ghostPos[i], goal);
        if (path.size() > 1) {
            // Check if the next position is occupied by another ghost
            bool occupied = false;
            for (int j = 0; j < 3; j++) {
                if (j != i && ghostPos[j] == path[1]) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                ghostPos[i] = path[1]; // Move to next position
            }
        }
    }
}

// Check collision
bool checkCollision() {
    for (auto& gp : ghostPos) {
        if (gp == pacmanPos) {
            return true;
        }
    }
    return false;
}

// Function to place a T shape wall
void placeTShape(int i, int j) {
    if (i + 1 < MSZ && j - 1 >= 0 && j + 1 < MSZ) {
        maze[i][j] = WALL;
        maze[i][j-1] = WALL;
        maze[i][j+1] = WALL;
        maze[i+1][j] = WALL;
    }
}

// Function to place an H shape wall
void placeHShape(int i, int j) {
    if (i + 2 < MSZ && j - 1 >= 0 && j + 1 < MSZ) {
        maze[i][j] = WALL;
        maze[i+1][j] = WALL;
        maze[i+2][j] = WALL;
        maze[i+1][j-1] = WALL;
        maze[i+1][j+1] = WALL;
    }
}

// Function to place an S shape wall
void placeSShape(int i, int j) {
    if (i + 1 < MSZ && j + 2 < MSZ) {
        maze[i][j] = WALL;
        maze[i][j+1] = WALL;
        maze[i+1][j+1] = WALL;
        maze[i+1][j+2] = WALL;
    }
}

// Function to check if maze is connected (Pac-Man can reach all coins)
bool isMazeConnected() {
    vector<vector<bool>> visited(MSZ, vector<bool>(MSZ, false));
    queue<Position> q;
    q.push(pacmanPos);
    visited[pacmanPos.row][pacmanPos.col] = true;
    int reachableCoins = 0;
    while (!q.empty()) {
        Position p = q.front(); q.pop();
        auto it = find(coins.begin(), coins.end(), p);
        if (it != coins.end()) reachableCoins++;
        for (auto& d : directions) {
            int nr = p.row + d[0], nc = p.col + d[1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL && !visited[nr][nc]) {
                visited[nr][nc] = true;
                q.push(Position(nr, nc));
            }
        }
    }
    return reachableCoins == (int)coins.size();
}

// Initialize maze
void initMaze() {
    static int regenAttempts = 0;
    regenAttempts++;
    
    srand(time(NULL)); // Randomize for each game
    
    // Create a simple maze with walls
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            if (i == 0 || i == MSZ-1 || j == 0 || j == MSZ-1) {
                maze[i][j] = WALL; // Solid boundaries
            } else if ((i % 2 == 0 && j % 2 == 0)) {
                maze[i][j] = WALL; // Basic internal walls
            } else if (rand() % 8 == 0) {
                // Check if any diagonal neighbor is a wall to avoid diagonal touching
                bool hasDiagonalWall = false;
                if (i > 0 && j > 0 && maze[i-1][j-1] == WALL) hasDiagonalWall = true;
                if (i > 0 && j < MSZ-1 && maze[i-1][j+1] == WALL) hasDiagonalWall = true;
                if (i < MSZ-1 && j > 0 && maze[i+1][j-1] == WALL) hasDiagonalWall = true;
                if (i < MSZ-1 && j < MSZ-1 && maze[i+1][j+1] == WALL) hasDiagonalWall = true;
                if (!hasDiagonalWall) {
                    maze[i][j] = WALL;
                } else {
                    maze[i][j] = SPACE;
                }
            } else {
                maze[i][j] = SPACE;
            }
        }
    }

    // Add shaped walls randomly
    for (int attempt = 0; attempt < 10; attempt++) { // Try 10 times to place shapes
        int i = rand() % (MSZ - 5) + 2; // Avoid edges
        int j = rand() % (MSZ - 5) + 2;
        int shape = rand() % 3; // 0: T, 1: H, 2: S
        if (maze[i][j] == SPACE) { // Only place if starting position is space
            if (shape == 0) placeTShape(i, j);
            else if (shape == 1) placeHShape(i, j);
            else if (shape == 2) placeSShape(i, j);
        }
    }

    // Add periodic walls inside perimeter to break loops
    for (int i = 1; i < MSZ-1; i += 5) {
        if (maze[1][i] == SPACE) maze[1][i] = WALL; // Top inner
        if (maze[MSZ-2][i] == SPACE) maze[MSZ-2][i] = WALL; // Bottom inner
        if (maze[i][1] == SPACE) maze[i][1] = WALL; // Left inner
        if (maze[i][MSZ-2] == SPACE) maze[i][MSZ-2] = WALL; // Right inner
    }

    // Place Pac-Man
    pacmanPos = Position(MSZ/2, MSZ/2);

    // Place ghosts at random positions
    vector<Position> availableSpaces;
    for (int i = 1; i < MSZ-1; i++) {
        for (int j = 1; j < MSZ-1; j++) {
            if (maze[i][j] == SPACE && (i != MSZ/2 || j != MSZ/2)) { // Not Pac-Man's position
                availableSpaces.push_back(Position(i, j));
            }
        }
    }
    
    // Shuffle and pick 3 positions for ghosts
    std::shuffle(availableSpaces.begin(), availableSpaces.end(), std::mt19937{std::random_device{}()});
    for (int i = 0; i < 3 && i < availableSpaces.size(); i++) {
        ghostPos[i] = availableSpaces[i];
    }

    // Clear and place coins - one on every floor space
    coins.clear();
    for (int i = 1; i < MSZ-1; i++) {
        for (int j = 1; j < MSZ-1; j++) {
            if (maze[i][j] == SPACE) {
                coins.push_back(Position(i, j));
            }
        }
    }
    initialCoins = coins.size();

    // Check connectivity, regenerate if not connected (with limit to prevent infinite loop)
    if (!isMazeConnected() && regenAttempts < 10) {
        initMaze(); // Recurse to regenerate
    } else {
        regenAttempts = 0; // Reset for next game
        // If still not connected after 10 attempts, proceed anyway to avoid freeze
    }
}

// Reset game for new run
void resetGame() {
    initMaze();
    currentState = PLAYING;
}

// Draw text on screen
void drawText(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (currentState == MENU) {
        glColor3d(1, 1, 1);
        drawText(-0.3, 0.2, "Pac-Man AI Game");
        drawText(-0.4, 0, "Left-click to start");
        drawText(-0.5, -0.2, "Ghosts use A* to chase");
        drawText(-0.5, -0.4, "Pac-Man uses BFS to evade");
    } else if (currentState == PLAYING) {
        double cellSize = 2.0 / MSZ;

        for (int i = 0; i < MSZ; i++) {
            for (int j = 0; j < MSZ; j++) {
                double x = -1 + j * cellSize;
                double y = 1 - i * cellSize;

                if (maze[i][j] == WALL) {
                    glColor3d(0, 0, 0);
                } else {
                    glColor3d(0.2, 0.2, 0.2); // Dark gray for better contrast with yellow coins and Pacman
                }

                glBegin(GL_QUADS);
                glVertex2d(x, y);
                glVertex2d(x + cellSize, y);
                glVertex2d(x + cellSize, y - cellSize);
                glVertex2d(x, y - cellSize);
                glEnd();
            }
        }

        // Draw coins
        glColor3d(1, 1, 0);
        glPointSize(5);
        glBegin(GL_POINTS);
        for (auto& coin : coins) {
            double x = -1 + coin.col * cellSize + cellSize/2;
            double y = 1 - coin.row * cellSize - cellSize/2;
            glVertex2d(x, y);
        }
        glEnd();

        // Draw Pac-Man
        glColor3d(1, 1, 0);
        double px = -1 + pacmanPos.col * cellSize + cellSize/2;
        double py = 1 - pacmanPos.row * cellSize - cellSize/2;
        glPushMatrix();
        glTranslatef(px, py, 0);
        glutSolidSphere(cellSize/2 * 0.8, 10, 10);
        glPopMatrix();

        // Draw ghosts
        for (int i = 0; i < 3; i++) {
            if (i == 0) glColor3d(1, 0, 0);
            else if (i == 1) glColor3d(0, 1, 0);
            else glColor3d(0, 0, 1);
            double gx = -1 + ghostPos[i].col * cellSize + cellSize/2;
            double gy = 1 - ghostPos[i].row * cellSize - cellSize/2;
            glPushMatrix();
            glTranslatef(gx, gy, 0);
            glutSolidSphere(cellSize/2 * 0.8, 10, 10);
            glPopMatrix();
        }

        // Draw score
        glColor3d(1, 1, 1);
        char scoreText[50];
        sprintf(scoreText, "Coins left: %d", (int)coins.size());
        drawText(-0.95f, 0.95f, scoreText);  // Dynamic position: top-left corner, slightly inset
    } else if (currentState == GAME_OVER) {
        glColor3d(1, 0, 0);
        drawText(-0.2, 0.2, "GAME OVER");
        char scoreText[50];
        sprintf(scoreText, "Final score: %d", initialCoins - (int)coins.size());
        drawText(-0.3, 0, scoreText);
        drawText(-0.4, -0.2, "Left-click to restart");
    } else if (currentState == WIN) {
        glColor3d(0, 1, 0);
        drawText(-0.15, 0.2, "YOU WIN!");
        char scoreText[50];
        sprintf(scoreText, "Final score: %d", initialCoins);
        drawText(-0.3, 0, scoreText);
        drawText(-0.4, -0.2, "Left-click to restart");
    }

    glutSwapBuffers();
}

// Mouse callback for menu interaction
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (currentState == MENU || currentState == GAME_OVER || currentState == WIN) {
            resetGame();
        }
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    // initMaze(); // Removed - will be called in resetGame

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Pac-Man AI");

    // GLEW initialization removed - not needed for basic GLUT functionality

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);

    glutMainLoop();
    return 0;
}