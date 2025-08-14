
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "glut.h"

#include "Cell.h"
#include <iostream>

#include <queue>
#include <stack>
#include <vector>

using namespace std;

const int WIDTH = 600;
const int HEIGHT = 600;

const int MSZ = 100;

const int SPACE = 0;
const int WALL = 1;
const int START = 2;
const int TARGET = 3;
const int BLACK = 4;
const int GRAY = 5;
const int PATH = 6;
const int GRAY_FORWARD = 7;    // Forward search frontier
const int GRAY_BACKWARD = 8;   // Backward search frontier  
const int BLACK_FORWARD = 9;   // Forward search visited
const int BLACK_BACKWARD = 10; // Backward search visited
const int MEETING_POINT = 11;  // Where searches meet
const int IS_FREE = -1;
const int UP = 1;
const int DOWN = 2;
const int LEFT = 3;
const int RIGHT = 0;

bool startBFS = false;
bool startDFS = false;
bool startBidirectionalBFS = false;

queue<Cell *> grays;
vector<Cell *> dfsGrays;
queue<Cell *> forwardQueue;   // BFS from START
queue<Cell *> backwardQueue;  // BFS from TARGET

// Store cells by position for bidirectional search
Cell* forwardCells[MSZ][MSZ];   // Forward search cells by position
Cell* backwardCells[MSZ][MSZ];  // Backward search cells by position

int maze[MSZ][MSZ] = {0};

void InitMaze()
{
	int i, j;

	// set frame
	for (i = 0; i < MSZ; i++)
	{
		maze[0][i] = WALL;
		maze[i][0] = WALL;
		maze[MSZ - 1][i] = WALL;
		maze[i][MSZ - 1] = WALL;
	}

	// setup walls
	for (i = 1; i < MSZ - 1; i++)
		for (j = 1; j < MSZ - 1; j++)
		{
			if (i % 2 == 0) // mostly WALLS
			{
				if (rand() % 100 < 75)
					maze[i][j] = WALL;
				else
					maze[i][j] = SPACE;
			}
			else // mostly SPACES
			{
				if (rand() % 100 < 85)
					maze[i][j] = SPACE;
				else
					maze[i][j] = WALL;
			}
		}

	maze[MSZ / 2][MSZ / 2] = START;
	
	// Ensure TARGET is placed on a SPACE, not a WALL
	int targetRow, targetCol;
	do {
		targetRow = rand() % MSZ;
		targetCol = rand() % MSZ;
	} while (maze[targetRow][targetCol] != SPACE);
	
	maze[targetRow][targetCol] = TARGET;
}

void init()
{
	srand(time(0));

	glClearColor(0.3, 0.3, 0.3, 0); // color of window background
	glOrtho(0, MSZ, 0, MSZ, -1, 1); // set the coordinates system

	InitMaze();
}

void ShowMaze()
{
	int i, j;

	for (i = 0; i < MSZ; i++)
		for (j = 0; j < MSZ; j++)
		{
			switch (maze[i][j])
			{
			case SPACE:
				glColor3d(0.8, 0.8, 0.8); // light gray
				break;
			case WALL:
				glColor3d(0.3, 0, 0); // dark red
				break;
			case START:
				glColor3d(0.2, 0.4, 1); // blue
				break;
			case TARGET:
				glColor3d(1, 0, 0); // red
				break;
			case BLACK:
				glColor3d(0.5, 0.5, 0.5); // dark gray
				break;
			case GRAY:
				glColor3d(0, 1, 0); //  green
				break;
			case PATH:
				glColor3d(1, 0, 1); //  magenta
				break;
			case GRAY_FORWARD:
				glColor3d(0.5, 1, 0.5); // light green (forward frontier)
				break;
			case GRAY_BACKWARD:
				glColor3d(1, 0.5, 0.5); // light red (backward frontier)
				break;
			case BLACK_FORWARD:
				glColor3d(0.2, 0.6, 0.2); // dark green (forward visited)
				break;
			case BLACK_BACKWARD:
				glColor3d(0.6, 0.2, 0.2); // dark red (backward visited)
				break;
			case MEETING_POINT:
				glColor3d(1, 1, 0); // yellow (meeting point)
				break;
			}
			// draw square
			glBegin(GL_POLYGON);
			glVertex2d(j, i);
			glVertex2d(j, i + 1);
			glVertex2d(j + 1, i + 1);
			glVertex2d(j + 1, i);
			glEnd();
		}
}

void RestorePath(Cell *pc)
{
	while (pc != nullptr)
	{
		maze[pc->getRow()][pc->getCol()] = PATH;
		pc = pc->getParent();
	}
}

void RestoreBidirectionalPath(Cell *forwardCell, Cell *backwardCell)
{
	// Restore forward path (from START to meeting point)
	Cell *current = forwardCell;
	while (current != nullptr)
	{
		maze[current->getRow()][current->getCol()] = PATH;
		current = current->getParent();
	}
	
	// Restore backward path (from meeting point to TARGET)
	current = backwardCell;
	while (current != nullptr)
	{
		maze[current->getRow()][current->getCol()] = PATH;
		current = current->getParent();
	}
}

void ResetMaze()
{
	// Clear all queues/stacks
	while (!grays.empty()) grays.pop();
	dfsGrays.clear();
	while (!forwardQueue.empty()) forwardQueue.pop();
	while (!backwardQueue.empty()) backwardQueue.pop();
	
	// Clear the cell position arrays
	for (int i = 0; i < MSZ; i++)
	{
		for (int j = 0; j < MSZ; j++)
		{
			forwardCells[i][j] = nullptr;
			backwardCells[i][j] = nullptr;
		}
	}
	
	// Reset maze colors to original state
	for (int i = 0; i < MSZ; i++)
	{
		for (int j = 0; j < MSZ; j++)
		{
			if (maze[i][j] == BLACK || maze[i][j] == GRAY || maze[i][j] == PATH ||
				maze[i][j] == GRAY_FORWARD || maze[i][j] == GRAY_BACKWARD ||
				maze[i][j] == BLACK_FORWARD || maze[i][j] == BLACK_BACKWARD ||
				maze[i][j] == MEETING_POINT)
			{
				maze[i][j] = SPACE;
			}
		}
	}
}

bool CheckNeighbor(int row, int col, Cell *pCurrent, bool isBFS)
{
	if (maze[row][col] == TARGET)
	{
		if (isBFS)
			startBFS = false;
		else
			startDFS = false;
		cout << "The solution has been found\n";
		RestorePath(pCurrent);
		return false;
	}
	else // if the neighbor is WHITE
	{
		Cell *pn = new Cell(row, col, pCurrent);
		maze[row][col] = GRAY;
		if (isBFS)
			grays.push(pn);
		else
			dfsGrays.push_back(pn);
		return true;
	}
}

bool CheckNeighborBidirectional(int row, int col, Cell *pCurrent, bool isForward)
{
	int currentCellType = maze[row][col];
	
	// Check if we've found the target (for forward search)
	if (isForward && currentCellType == TARGET)
	{
		startBidirectionalBFS = false;
		cout << "Forward search found TARGET\n";
		RestorePath(pCurrent);
		return false;
	}
	
	// Check if we've found the start (for backward search)  
	if (!isForward && currentCellType == START)
	{
		startBidirectionalBFS = false;
		cout << "Backward search found START\n";
		RestorePath(pCurrent);
		return false;
	}
	
	// Check if the searches have met
	if (isForward && (currentCellType == GRAY_BACKWARD || currentCellType == BLACK_BACKWARD))
	{
		startBidirectionalBFS = false;
		maze[row][col] = MEETING_POINT;
		cout << "Bidirectional search: Forward met Backward at (" << row << "," << col << ")\n";
		
		// Find the corresponding backward cell at this position
		Cell *backwardCell = backwardCells[row][col];
		if (backwardCell != nullptr)
		{
			RestoreBidirectionalPath(pCurrent, backwardCell);
		}
		else
		{
			RestorePath(pCurrent); // Fallback
		}
		return false;
	}
	
	if (!isForward && (currentCellType == GRAY_FORWARD || currentCellType == BLACK_FORWARD))
	{
		startBidirectionalBFS = false;
		maze[row][col] = MEETING_POINT;
		cout << "Bidirectional search: Backward met Forward at (" << row << "," << col << ")\n";
		
		// Find the corresponding forward cell at this position
		Cell *forwardCell = forwardCells[row][col];
		if (forwardCell != nullptr)
		{
			RestoreBidirectionalPath(forwardCell, pCurrent);
		}
		else
		{
			RestorePath(pCurrent); // Fallback
		}
		return false;
	}
	
	// If it's a free space, add to appropriate queue
	if (currentCellType == SPACE)
	{
		Cell *pn = new Cell(row, col, pCurrent);
		if (isForward)
		{
			maze[row][col] = GRAY_FORWARD;
			forwardQueue.push(pn);
			forwardCells[row][col] = pn; // Store for later lookup
		}
		else
		{
			maze[row][col] = GRAY_BACKWARD;
			backwardQueue.push(pn);
			backwardCells[row][col] = pn; // Store for later lookup
		}
		return true;
	}
	
	return true;
}

void BFSIteration()
{
	Cell *pCurrent;
	int row, col;
	bool go_on = true;
	if (grays.empty())
	{
		startBFS = false;
		cout << "There is no solution: GRAYS is empty\n";
		return;
	}
	else // grays is not empty
	{
		// 1. Extract first cell from grays
		pCurrent = grays.front();
		grays.pop();
		// 2. paint it BLACK
		row = pCurrent->getRow();
		col = pCurrent->getCol();
		if (maze[row][col] != START)
			maze[row][col] = BLACK;
		// 3. search for WHITE neighbors of pCurrent
		int directions[4] = {IS_FREE, IS_FREE, IS_FREE, IS_FREE};

		int index;
		for (int dir = 0; dir < 4; dir++)
		{
			do
			{
				index = rand() % 4;
			} while (directions[index] != IS_FREE);
			directions[index] = dir;
		}

		for (int i = 0; i < 4; i++)
		{
			int lookupDirection = directions[i];
			switch(lookupDirection)
			{
				case UP:
				if (go_on && (maze[row + 1][col] == SPACE || maze[row + 1][col] == TARGET))
					go_on = CheckNeighbor(row + 1, col, pCurrent, true);
				break;
				case DOWN:
				if (go_on && (maze[row - 1][col] == SPACE || maze[row - 1][col] == TARGET))
					go_on = CheckNeighbor(row - 1, col, pCurrent, true);
				break;
				case LEFT:
				if (go_on && (maze[row][col - 1] == SPACE || maze[row][col - 1] == TARGET))
					go_on = CheckNeighbor(row, col - 1, pCurrent, true);
				break;
				case RIGHT:
				if (go_on && (maze[row][col + 1] == SPACE || maze[row][col + 1] == TARGET))
					go_on = CheckNeighbor(row, col + 1, pCurrent, true);
				break;
			}
		}
	}
}

void DFSIteration()
{
	Cell *pCurrent;
	int row, col;
	bool go_on = true;
	if (dfsGrays.empty())
	{
		startDFS = false;
		cout << "There is no solution: DFS GRAYS is empty\n";
		return;
	}
	else // dfsGrays is not empty
	{
		// 1. Extract last cell from dfsGrays (stack behavior with vector)
		pCurrent = dfsGrays.back(); // Get last element
		dfsGrays.pop_back();		// Remove last element
		// 2. paint it BLACK
		row = pCurrent->getRow();
		col = pCurrent->getCol();
		if (maze[row][col] != START)
			maze[row][col] = BLACK;
		// 3. search for WHITE neighbors of pCurrent
		// In random direction
		// check UP
		if (maze[row + 1][col] == SPACE || maze[row + 1][col] == TARGET)
			go_on = CheckNeighbor(row + 1, col, pCurrent, false);
		// check DOWN
		if (go_on && (maze[row - 1][col] == SPACE || maze[row - 1][col] == TARGET))
			go_on = CheckNeighbor(row - 1, col, pCurrent, false);
		// check LEFT
		if (go_on && (maze[row][col - 1] == SPACE || maze[row][col - 1] == TARGET))
			go_on = CheckNeighbor(row, col - 1, pCurrent, false);
		// check Right
		if (go_on && (maze[row][col + 1] == SPACE || maze[row][col + 1] == TARGET))
			go_on = CheckNeighbor(row, col + 1, pCurrent, false);
	}
}

void BidirectionalBFSIteration()
{
	bool go_on = true;
	
	// Check if both queues are empty
	if (forwardQueue.empty() && backwardQueue.empty())
	{
		startBidirectionalBFS = false;
		cout << "No solution: Both queues are empty\n";
		return;
	}
	
	// Process forward search (from START)
	if (!forwardQueue.empty() && go_on)
	{
		Cell *pCurrent = forwardQueue.front();
		forwardQueue.pop();
		
		int row = pCurrent->getRow();
		int col = pCurrent->getCol();
		
		if (maze[row][col] != START)
			maze[row][col] = BLACK_FORWARD;
		
		// Store the cell for potential meeting point lookup
		forwardCells[row][col] = pCurrent;
			
		// Check all 4 neighbors for forward search
		if (go_on && (maze[row + 1][col] == SPACE || maze[row + 1][col] == TARGET || 
					  maze[row + 1][col] == GRAY_BACKWARD || maze[row + 1][col] == BLACK_BACKWARD))
			go_on = CheckNeighborBidirectional(row + 1, col, pCurrent, true);
		if (go_on && (maze[row - 1][col] == SPACE || maze[row - 1][col] == TARGET ||
					  maze[row - 1][col] == GRAY_BACKWARD || maze[row - 1][col] == BLACK_BACKWARD))
			go_on = CheckNeighborBidirectional(row - 1, col, pCurrent, true);
		if (go_on && (maze[row][col - 1] == SPACE || maze[row][col - 1] == TARGET ||
					  maze[row][col - 1] == GRAY_BACKWARD || maze[row][col - 1] == BLACK_BACKWARD))
			go_on = CheckNeighborBidirectional(row, col - 1, pCurrent, true);
		if (go_on && (maze[row][col + 1] == SPACE || maze[row][col + 1] == TARGET ||
					  maze[row][col + 1] == GRAY_BACKWARD || maze[row][col + 1] == BLACK_BACKWARD))
			go_on = CheckNeighborBidirectional(row, col + 1, pCurrent, true);
	}
	
	// Process backward search (from TARGET)
	if (!backwardQueue.empty() && go_on)
	{
		Cell *pCurrent = backwardQueue.front();
		backwardQueue.pop();
		
		int row = pCurrent->getRow();
		int col = pCurrent->getCol();
		
		if (maze[row][col] != TARGET)
			maze[row][col] = BLACK_BACKWARD;
		
		// Store the cell for potential meeting point lookup
		backwardCells[row][col] = pCurrent;
			
		// Check all 4 neighbors for backward search
		if (go_on && (maze[row + 1][col] == SPACE || maze[row + 1][col] == START ||
					  maze[row + 1][col] == GRAY_FORWARD || maze[row + 1][col] == BLACK_FORWARD))
			go_on = CheckNeighborBidirectional(row + 1, col, pCurrent, false);
		if (go_on && (maze[row - 1][col] == SPACE || maze[row - 1][col] == START ||
					  maze[row - 1][col] == GRAY_FORWARD || maze[row - 1][col] == BLACK_FORWARD))
			go_on = CheckNeighborBidirectional(row - 1, col, pCurrent, false);
		if (go_on && (maze[row][col - 1] == SPACE || maze[row][col - 1] == START ||
					  maze[row][col - 1] == GRAY_FORWARD || maze[row][col - 1] == BLACK_FORWARD))
			go_on = CheckNeighborBidirectional(row, col - 1, pCurrent, false);
		if (go_on && (maze[row][col + 1] == SPACE || maze[row][col + 1] == START ||
					  maze[row][col + 1] == GRAY_FORWARD || maze[row][col + 1] == BLACK_FORWARD))
			go_on = CheckNeighborBidirectional(row, col + 1, pCurrent, false);
	}
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT); // clean frame buffer

	ShowMaze();

	glutSwapBuffers(); // show all
}

void displayGraph()
{
	glClear(GL_COLOR_BUFFER_BIT); // clean frame buffer

	ShowMaze();

	glutSwapBuffers(); // show all
}
void idle()
{
	if (startBFS)
		BFSIteration();
	if (startDFS)
		DFSIteration();
	if (startBidirectionalBFS)
		BidirectionalBFSIteration();

	glutPostRedisplay();
}

void menu(int choice)
{
	// Reset maze before starting new algorithm
	ResetMaze();

	switch (choice)
	{
	case 1: // BFS
		glutDisplayFunc(display);
		startBFS = true;
		startDFS = false;
		startBidirectionalBFS = false;
		{
			Cell *pc = new Cell(MSZ / 2, MSZ / 2, nullptr);
			grays.push(pc);
		}
		break;
	case 2: // DFS
		glutDisplayFunc(display);
		startDFS = true;
		startBFS = false;
		startBidirectionalBFS = false;
		{
			Cell *pc = new Cell(MSZ / 2, MSZ / 2, nullptr);
			dfsGrays.push_back(pc);
		}
		break;
	case 3: // Bidirectional BFS
		glutDisplayFunc(display);
		startBidirectionalBFS = true;
		startBFS = false;
		startDFS = false;
		{
			// Find START and TARGET positions
			int startRow = MSZ / 2, startCol = MSZ / 2;
			int targetRow = -1, targetCol = -1;
			
			// Find TARGET position
			for (int i = 0; i < MSZ && targetRow == -1; i++)
			{
				for (int j = 0; j < MSZ && targetCol == -1; j++)
				{
					if (maze[i][j] == TARGET)
					{
						targetRow = i;
						targetCol = j;
					}
				}
			}
			
			// Initialize both searches
			Cell *forwardStart = new Cell(startRow, startCol, nullptr);
			Cell *backwardStart = new Cell(targetRow, targetCol, nullptr);
			
			forwardQueue.push(forwardStart);
			backwardQueue.push(backwardStart);
		}
		break;
		case 4: // UCS
			glutDisplayFunc(displayGraph);
		break; 
	}
}

int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutInitWindowPosition(400, 100);
	glutCreateWindow("Maze");

	// display is the refresh function
	glutDisplayFunc(display);
	// idle is the background function
	glutIdleFunc(idle);
	// menu:
	glutCreateMenu(menu);
	glutAddMenuEntry("BFS", 1);
	glutAddMenuEntry("DFS", 2);
	glutAddMenuEntry("BFS bidirectional", 3);
	glutAddMenuEntry("UCS", 4);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	init();

	glutMainLoop();

	return 0;
}