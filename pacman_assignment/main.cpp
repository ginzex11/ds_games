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

const int MSZ = 20; // Smaller maze for visibility

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

double heuristic(Position a, Position b) {
    return abs(a.row - b.row) + abs(a.col - b.col); // Manhattan
}

// Idle function for game loop
void idle() {
    if (currentState == PLAYING) {
        movePacman();
        moveGhosts();
        if (checkCollision()) {
            cout << "Game Over! Final score: " << (initialCoins - (int)coins.size()) << endl;
            currentState = GAME_OVER;
        }
        if (coins.empty()) {
            cout << "You Win! Final score: " << initialCoins << endl;
            currentState = WIN;
        }
        glutPostRedisplay();
        Sleep(100); // Faster frame rate
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

    while (!openList.empty()) {
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

// BFS limited depth for Pac-Man
Position bfsLimited(Position start, int maxDepth) {
    queue<pair<Position, int>> q;
    vector<vector<bool>> visited(MSZ, vector<bool>(MSZ, false));
    q.push({start, 0});
    visited[start.row][start.col] = true;

    while (!q.empty()) {
        auto [curr, depth] = q.front();
        q.pop();

        if (depth >= maxDepth) continue;

        for (auto& dir : directions) {
            int nr = curr.row + dir[0];
            int nc = curr.col + dir[1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL && !visited[nr][nc]) {
                visited[nr][nc] = true;
                q.push({Position(nr, nc), depth + 1});
                
                // Check if ghost is here
                for (auto& gp : ghostPos) {
                    if (gp.row == nr && gp.col == nc) {
                        // Found ghost, calculate position away from it
                        int dr = nr - start.row;
                        int dc = nc - start.col;
                        int away_r = start.row - dr;
                        int away_c = start.col - dc;
                        
                        // Make sure the away position is valid
                        if (away_r >= 0 && away_r < MSZ && away_c >= 0 && away_c < MSZ && maze[away_r][away_c] != WALL) {
                            return Position(away_r, away_c);
                        } else {
                            // If away position is invalid, try to find a safe adjacent position
                            for (auto& dir2 : directions) {
                                int safe_r = start.row + dir2[0];
                                int safe_c = start.col + dir2[1];
                                if (safe_r >= 0 && safe_r < MSZ && safe_c >= 0 && safe_c < MSZ && maze[safe_r][safe_c] != WALL) {
                                    bool safe = true;
                                    for (auto& gp2 : ghostPos) {
                                        if (abs(gp2.row - safe_r) + abs(gp2.col - safe_c) <= 2) {
                                            safe = false;
                                            break;
                                        }
                                    }
                                    if (safe) return Position(safe_r, safe_c);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return Position(-1, -1); // No ghost found
}

// Find nearest coin
Position nearestCoin(Position start) {
    Position nearest = {-1, -1};
    double minDist = 1e9;
    for (auto& coin : coins) {
        double dist = abs(coin.row - start.row) + abs(coin.col - start.col);
        if (dist < minDist) {
            minDist = dist;
            nearest = coin;
        }
    }
    return nearest;
}

// Move Pac-Man
void movePacman() {
    Position avoid = bfsLimited(pacmanPos, 5); // Limited depth 5
    Position target;
    if (avoid.row != -1) {
        target = avoid;
    } else {
        target = nearestCoin(pacmanPos);
    }

    if (target.row == -1) return; // No target

    // Simple move towards target
    int dr = target.row - pacmanPos.row;
    int dc = target.col - pacmanPos.col;
    int nr = pacmanPos.row + (dr > 0 ? 1 : (dr < 0 ? -1 : 0));
    int nc = pacmanPos.col + (dc > 0 ? 1 : (dc < 0 ? -1 : 0));

    if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
        pacmanPos = Position(nr, nc);
        // Check coin collection
        auto it = find(coins.begin(), coins.end(), pacmanPos);
        if (it != coins.end()) {
            coins.erase(it);
        }
    } else {
        // If target direction blocked, find alternative safe move
        Position bestMove = pacmanPos;
        double maxDist = 0;
        for (auto& dir : directions) {
            int nnr = pacmanPos.row + dir[0];
            int nnc = pacmanPos.col + dir[1];
            if (nnr >= 0 && nnr < MSZ && nnc >= 0 && nnc < MSZ && maze[nnr][nnc] != WALL) {
                double minGhostDist = 1e9;
                for (auto& gp : ghostPos) {
                    double dist = abs(gp.row - nnr) + abs(gp.col - nnc);
                    if (dist < minGhostDist) minGhostDist = dist;
                }
                if (minGhostDist > maxDist) {
                    maxDist = minGhostDist;
                    bestMove = Position(nnr, nnc);
                }
            }
        }
        if (bestMove != pacmanPos) {
            pacmanPos = bestMove;
            auto it = find(coins.begin(), coins.end(), pacmanPos);
            if (it != coins.end()) {
                coins.erase(it);
            }
        }
    }
}

// Move ghosts
void moveGhosts() {
    for (int i = 0; i < 3; i++) {
        vector<Position> path = aStar(ghostPos[i], pacmanPos);
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

// Initialize maze
void initMaze() {
    srand(time(NULL)); // Randomize for each game
    
    // Create a simple maze with walls
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            if (i == 0 || i == MSZ-1 || j == 0 || j == MSZ-1 || (i % 2 == 0 && j % 2 == 0)) {
                maze[i][j] = WALL;
            } else {
                maze[i][j] = SPACE;
            }
        }
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

    // Clear and place coins
    coins.clear();
    for (int i = 1; i < MSZ-1; i++) {
        for (int j = 1; j < MSZ-1; j++) {
            if (maze[i][j] == SPACE) {
                coins.push_back(Position(i, j));
            }
        }
    }
    initialCoins = coins.size();
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
                    glColor3d(1, 1, 1);
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
        drawText(-0.9, 0.9, scoreText);
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