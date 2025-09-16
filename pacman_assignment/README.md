# Pac-Man AI Game

An AI vs AI implementation of Pac-Man where ghosts use A* pathfinding to chase Pac-Man, and Pac-Man uses depth-limited BFS to avoid ghosts and collect coins.

## Features
- **Ghost AI**: A* algorithm with Manhattan heuristic for optimal chasing
- **Pac-Man AI**: Depth-limited BFS (depth 5) for ghost avoidance, falls back to nearest coin collection
- **Real-time Gameplay**: Automated game with no human input required
- **Visual Rendering**: OpenGL/GLUT graphics showing maze, entities, and coins
- **Score Display**: On-screen counter for remaining coins

## Build Instructions

### Windows (MSYS2/MinGW)
1. Install MSYS2 and required packages:
   ```
   pacman -S mingw-w64-x86_64-freeglut mingw-w64-x86_64-glew
   ```

2. Compile:
   ```
   g++ -o Pacman.exe main.cpp Cell.cpp -lfreeglut -lglew32 -lopengl32 -lglu32 -lgdi32 -lwinmm -luser32 -I.
   ```

3. Run:
   ```
   ./Pacman.exe
   ```

### Visual Studio
1. Open `Pacman.vcxproj` in Visual Studio
2. Build and run the project

## Controls
- No user input required - the game runs automatically
- Close the window to exit

## Game Rules
- Pac-Man collects yellow coins while avoiding red/green/blue ghosts
- Ghosts chase Pac-Man using A* pathfinding
- Pac-Man evades ghosts using BFS within a limited search depth
- Game ends when Pac-Man is caught or all coins are collected

## Technical Details
- Maze size: 20x20 grid
- Algorithms: A* for ghosts, BFS (depth-limited) for Pac-Man
- Graphics: OpenGL with GLUT for rendering
- Frame rate: ~10 FPS for smooth animation