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
#include <climits>
#include <set>

#include <GL/freeglut.h>
#include <GL/glut.h>

#include "Cell.h"
#include "Node.h"
#include "CompareNodes.h"

using namespace std;

const int WIDTH = 600;
const int HEIGHT = 600;
const int MSZ = 25;

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
    bool operator<(const Position& other) const {
        if (row != other.row) return row < other.row;
        return col < other.col;
    }
};

Position pacmanPos;
Position ghostPos[3];
vector<Position> coins;
int initialCoins;

int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; // 0: up, 1: down, 2: left, 3: right
int lastPacDir = -1;

void movePacman();
void moveGhosts();
bool checkCollision();
void initMaze();
void resetGame();
void drawText(float x, float y, const char* text);
void display();
void timer(int value);
void mouse(int button, int state, int x, int y);
void placeTShape(int i, int j);
void placeHShape(int i, int j);
void placeSShape(int i, int j);

double heuristic(Position a, Position b) {
    return abs(a.row - b.row) + abs(a.col - b.col);
}

Position predictPacman(Position pacPos, int lastDir) {
    Position pred = pacPos;
    if (lastDir == 0) pred.row--;
    else if (lastDir == 1) pred.row++;
    else if (lastDir == 2) pred.col--;
    else if (lastDir == 3) pred.col++;
    if (pred.row >= 0 && pred.row < MSZ && pred.col >= 0 && pred.col < MSZ && maze[pred.row][pred.col] != WALL) {
        return pred;
    }
    return pacPos;
}

// A* for ghosts with memory cleanup
vector<Position> aStar(Position start, Position goal) {
    set<Node*> allocatedNodes;
    
    priority_queue<Node*, vector<Node*>, CompareNodes> openList;
    vector<vector<bool>> closed(MSZ, vector<bool>(MSZ, false));
    vector<vector<Node*>> bestNodes(MSZ, vector<Node*>(MSZ, nullptr));

    Node* startNode = new Node(new Cell(start.row, start.col, nullptr), 0, heuristic(start, goal));
    allocatedNodes.insert(startNode);
    openList.push(startNode);
    bestNodes[start.row][start.col] = startNode;

    vector<Position> resultPath;
    int steps = 0;
    const int MAX_STEPS = MSZ * MSZ * 2;

    while (!openList.empty() && steps < MAX_STEPS) {
        steps++;
        Node* current = openList.top();
        openList.pop();

        Position currPos(current->getCell()->getRow(), current->getCell()->getCol());
        if (closed[currPos.row][currPos.col]) continue;

        if (currPos == goal) {
            while (current != nullptr) {
                resultPath.push_back(Position(current->getCell()->getRow(), current->getCell()->getCol()));
                current = current->getParent();
            }
            reverse(resultPath.begin(), resultPath.end());
            break;
        }

        closed[currPos.row][currPos.col] = true;

        for (int k = 0; k < 4; ++k) {
            int nr = currPos.row + directions[k][0];
            int nc = currPos.col + directions[k][1];
            
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL && !closed[nr][nc]) {
                double g = current->getG() + 1;
                double h = heuristic(Position(nr, nc), goal);
                
                if (bestNodes[nr][nc] == nullptr || g < bestNodes[nr][nc]->getG()) {
                    Node* neighbor = new Node(new Cell(nr, nc, nullptr), g, h, current);
                    allocatedNodes.insert(neighbor);
                    bestNodes[nr][nc] = neighbor;
                    openList.push(neighbor);
                }
            }
        }
    }

    for (Node* node : allocatedNodes) {
        delete node->getCell();
        delete node;
    }

    return resultPath;
}

// Limited-depth BFS for Pac-Man evasion
int bfsLimitedForEscape(Position start, int maxDepth) {
    struct State {
        Position pos;
        int depth;
        int first_dir;
    };

    queue<State> q;
    vector<vector<bool>> visited(MSZ, vector<bool>(MSZ, false));
    q.push({start, 0, -1});
    visited[start.row][start.col] = true;

    int min_dist = INT_MAX;
    int closest_first_dir = -1;
    Position closest_ghost = {-1, -1};

    while (!q.empty()) {
        State curr = q.front(); q.pop();

        for (int g = 0; g < 3; g++) {
            if (curr.pos == ghostPos[g] && curr.depth < min_dist) {
                min_dist = curr.depth;
                closest_first_dir = curr.first_dir;
                closest_ghost = curr.pos;
            }
        }

        if (curr.depth >= maxDepth) continue;

        for (int d = 0; d < 4; d++) {
            int nr = curr.pos.row + directions[d][0];
            int nc = curr.pos.col + directions[d][1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL && !visited[nr][nc]) {
                visited[nr][nc] = true;
                int fdir = (curr.depth == 0) ? d : curr.first_dir;
                q.push({{nr, nc}, curr.depth + 1, fdir});
            }
        }
    }

    if (min_dist <= maxDepth && closest_ghost.row != -1) {
        if (min_dist == 0) {
            double max_dist = -1;
            int best_dir = -1;
            for (int d = 0; d < 4; d++) {
                int nr = start.row + directions[d][0];
                int nc = start.col + directions[d][1];
                if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
                    double min_ghost_dist = 1e9;
                    for (int g = 0; g < 3; g++) {
                        if (ghostPos[g] != start) {
                            double dist = heuristic({nr, nc}, ghostPos[g]);
                            if (dist < min_ghost_dist) min_ghost_dist = dist;
                        }
                    }
                    if (min_ghost_dist > max_dist) {
                        max_dist = min_ghost_dist;
                        best_dir = d;
                    }
                }
            }
            return best_dir;
        }

        int opp_dir;
        if (closest_first_dir < 2) {
            opp_dir = 1 - closest_first_dir;
        } else {
            opp_dir = 5 - closest_first_dir;
        }

        int nr = start.row + directions[opp_dir][0];
        int nc = start.col + directions[opp_dir][1];
        if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
            return opp_dir;
        } else {
            double max_dist = -1;
            int best_dir = -1;
            for (int d = 0; d < 4; d++) {
                nr = start.row + directions[d][0];
                nc = start.col + directions[d][1];
                if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
                    double dist = heuristic({nr, nc}, closest_ghost);
                    if (dist > max_dist) {
                        max_dist = dist;
                        best_dir = d;
                    }
                }
            }
            return best_dir;
        }
    }

    return -1;
}

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

int getDirectionToTarget(Position from, Position to) {
    int dr = to.row - from.row;
    int dc = to.col - from.col;
    
    if (abs(dr) >= abs(dc)) {
        if (dr > 0) return 1;
        if (dr < 0) return 0;
    } else {
        if (dc > 0) return 3;
        if (dc < 0) return 2;
    }
    return -1;
}

void movePacman() {
    int escapeDir = bfsLimitedForEscape(pacmanPos, 5);
    int chosenDir = -1;
    
    if (escapeDir != -1) {
        chosenDir = escapeDir;
    } else {
        Position target = nearestCoin(pacmanPos);
        if (target.row != -1) {
            chosenDir = getDirectionToTarget(pacmanPos, target);
        }
    }
    
    if (chosenDir != -1) {
        int nr = pacmanPos.row + directions[chosenDir][0];
        int nc = pacmanPos.col + directions[chosenDir][1];
        
        if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
            pacmanPos = Position(nr, nc);
            lastPacDir = chosenDir;
            
            auto it = find(coins.begin(), coins.end(), pacmanPos);
            if (it != coins.end()) {
                coins.erase(it);
            }
            return;
        }
    }
    
    // Fallback with hysteresis: prefer perpendicular directions to last move to reduce back-forth
    int bestDir = -1;
    double maxMinGhostDist = -1;
    
    for (int dir = 0; dir < 4; dir++) {
        int nr = pacmanPos.row + directions[dir][0];
        int nc = pacmanPos.col + directions[dir][1];
        
        if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
            double minGhostDist = 1e9;
            for (int i = 0; i < 3; i++) {
                double dist = heuristic(Position(nr, nc), ghostPos[i]);
                if (dist < minGhostDist) minGhostDist = dist;
            }
            
            // Add small bonus for directions perpendicular to last (to break ties)
            double bonus = 0;
            if (lastPacDir != -1) {
                bool is_perp = abs(dir - lastPacDir) == 2 || (dir < 2 && lastPacDir >= 2) || (dir >= 2 && lastPacDir < 2);
                if (is_perp) bonus = 0.1;
            }
            
            double score = minGhostDist + bonus;
            
            if (score > maxMinGhostDist) {
                maxMinGhostDist = score;
                bestDir = dir;
            }
        }
    }
    
    if (bestDir == -1) {
        for (int dir = 0; dir < 4; dir++) {
            int nr = pacmanPos.row + directions[dir][0];
            int nc = pacmanPos.col + directions[dir][1];
            if (nr >= 0 && nr < MSZ && nc >= 0 && nc < MSZ && maze[nr][nc] != WALL) {
                bestDir = dir;
                break;
            }
        }
    }
    
    if (bestDir != -1) {
        int nr = pacmanPos.row + directions[bestDir][0];
        int nc = pacmanPos.col + directions[bestDir][1];
        pacmanPos = Position(nr, nc);
        lastPacDir = bestDir;
        
        auto it = find(coins.begin(), coins.end(), pacmanPos);
        if (it != coins.end()) {
            coins.erase(it);
        }
    }
}

void moveGhosts() {
    for (int i = 0; i < 3; i++) {
        Position goal = pacmanPos;
        if (i == 0) {
            goal = predictPacman(pacmanPos, lastPacDir);
        }
        vector<Position> path = aStar(ghostPos[i], goal);
        if (path.size() > 1) {
            bool occupied = false;
            for (int j = 0; j < 3; j++) {
                if (j != i && ghostPos[j] == path[1]) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                ghostPos[i] = path[1];
            }
        }
    }
}

bool checkCollision() {
    for (int i = 0; i < 3; i++) {
        if (ghostPos[i] == pacmanPos) {
            return true;
        }
    }
    return false;
}

void placeTShape(int i, int j) {
    if (i + 1 < MSZ && j - 1 >= 0 && j + 1 < MSZ) {
        maze[i][j] = WALL;
        maze[i][j-1] = WALL;
        maze[i][j+1] = WALL;
        maze[i+1][j] = WALL;
    }
}

void placeHShape(int i, int j) {
    if (i + 2 < MSZ && j - 1 >= 0 && j + 1 < MSZ) {
        maze[i][j] = WALL;
        maze[i+1][j] = WALL;
        maze[i+2][j] = WALL;
        maze[i+1][j-1] = WALL;
        maze[i+1][j+1] = WALL;
    }
}

void placeSShape(int i, int j) {
    if (i + 1 < MSZ && j + 2 < MSZ) {
        maze[i][j] = WALL;
        maze[i][j+1] = WALL;
        maze[i+1][j+1] = WALL;
        maze[i+1][j+2] = WALL;
    }
}

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

void initMaze() {
    int regenAttempts = 0;
    bool valid = false;
    while (!valid && regenAttempts < 10) {
        regenAttempts++;
        
        srand(time(NULL) + regenAttempts);
        
        for (int i = 0; i < MSZ; i++) {
            for (int j = 0; j < MSZ; j++) {
                if (i == 0 || i == MSZ-1 || j == 0 || j == MSZ-1) {
                    maze[i][j] = WALL;
                } else if ((i % 2 == 0 && j % 2 == 0)) {
                    maze[i][j] = WALL;
                } else if (rand() % 8 == 0) {
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

        for (int attempt = 0; attempt < 10; attempt++) {
            int i = rand() % (MSZ - 5) + 2;
            int j = rand() % (MSZ - 5) + 2;
            int shape = rand() % 3;
            if (maze[i][j] == SPACE) {
                if (shape == 0) placeTShape(i, j);
                else if (shape == 1) placeHShape(i, j);
                else placeSShape(i, j);
            }
        }

        for (int i = 1; i < MSZ-1; i += 5) {
            if (maze[1][i] == SPACE) maze[1][i] = WALL;
            if (maze[MSZ-2][i] == SPACE) maze[MSZ-2][i] = WALL;
            if (maze[i][1] == SPACE) maze[i][1] = WALL;
            if (maze[i][MSZ-2] == SPACE) maze[i][MSZ-2] = WALL;
        }

        pacmanPos = Position(MSZ/2, MSZ/2);

        vector<Position> availableSpaces;
        for (int i = 1; i < MSZ-1; i++) {
            for (int j = 1; j < MSZ-1; j++) {
                if (maze[i][j] == SPACE && (i != MSZ/2 || j != MSZ/2)) {
                    double dist = heuristic(Position(i, j), pacmanPos);
                    if (dist > 10) {
                        availableSpaces.push_back(Position(i, j));
                    }
                }
            }
        }

        if (availableSpaces.size() < 3) {
            continue;
        }
        
        std::shuffle(availableSpaces.begin(), availableSpaces.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < 3 && i < availableSpaces.size(); i++) {
            ghostPos[i] = availableSpaces[i];
        }

        coins.clear();
        for (int i = 1; i < MSZ-1; i++) {
            for (int j = 1; j < MSZ-1; j++) {
                if (maze[i][j] == SPACE) {
                    coins.push_back(Position(i, j));
                }
            }
        }
        initialCoins = coins.size();

        valid = isMazeConnected();
    }
}

void resetGame() {
    initMaze();
    currentState = PLAYING;
}

void drawText(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

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
                    glColor3d(0.2, 0.2, 0.2);
                }

                glBegin(GL_QUADS);
                glVertex2d(x, y);
                glVertex2d(x + cellSize, y);
                glVertex2d(x + cellSize, y - cellSize);
                glVertex2d(x, y - cellSize);
                glEnd();
            }
        }

        glColor3d(1, 1, 0);
        glPointSize(5);
        glBegin(GL_POINTS);
        for (auto& coin : coins) {
            double x = -1 + coin.col * cellSize + cellSize/2;
            double y = 1 - coin.row * cellSize - cellSize/2;
            glVertex2d(x, y);
        }
        glEnd();

        glColor3d(1, 1, 0);
        double px = -1 + pacmanPos.col * cellSize + cellSize/2;
        double py = 1 - pacmanPos.row * cellSize - cellSize/2;
        glPushMatrix();
        glTranslatef(px, py, 0);
        glutSolidSphere(cellSize/2 * 0.8, 10, 10);
        glPopMatrix();

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

        glColor3d(1, 1, 1);
        char scoreText[50];
        sprintf(scoreText, "Coins left: %d", (int)coins.size());
        drawText(-0.95f, 0.95f, scoreText);
    } else if (currentState == GAME_OVER) {
        glColor3d(1, 0, 0);
        drawText(-0.2, 0.2, "GAME OVER");
        char scoreText[50];
        sprintf(scoreText, "Final score: %d", initialCoins - (int)coins.size());
        drawText(-0.3, 0, scoreText);
        drawText(-0.5, -0.2, "Left-click or press R to restart, Q to quit");
    } else if (currentState == WIN) {
        glColor3d(0, 1, 0);
        drawText(-0.15, 0.2, "YOU WIN!");
        char scoreText[50];
        sprintf(scoreText, "Final score: %d", initialCoins);
        drawText(-0.3, 0, scoreText);
        drawText(-0.5, -0.2, "Left-click or press R to restart, Q to quit");
    }

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (currentState == MENU || currentState == GAME_OVER || currentState == WIN) {
            resetGame();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 'q' || key == 'Q') {
        exit(0);
    } else if (key == 'r' || key == 'R') {
        resetGame();
    }
}

void timer(int value) {
    static int frameCount = 0;
    frameCount++;
    if (currentState == PLAYING) {
        if (frameCount % 2 == 0) {
            movePacman();
        }
        if (frameCount % 3 == 0) {
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
    }
    glutPostRedisplay();
    glutTimerFunc(100, timer, 0);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Pac-Man AI");

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}