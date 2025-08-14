# Data Structures Games

A collection of pathfinding algorithm visualizations implemented in C++ using OpenGL/GLUT.

## Features

- **BFS (Breadth-First Search)**: Guarantees shortest path, explores layer by layer
- **DFS (Depth-First Search)**: May find longer paths, explores depth-first
- **Bidirectional BFS**: Two BFS searches meet in the middle for efficiency
- **UCS (Uniform Cost Search)**: Coming soon

## Visual Elements

- 🔵 **Blue**: Starting position
- 🔴 **Red**: Target position  
- 🟢 **Green**: Search frontier (BFS/DFS)
- ⚫ **Dark Gray**: Visited cells
- 🟪 **Magenta**: Final solution path
- 🟡 **Yellow**: Meeting point (Bidirectional BFS)

### Bidirectional BFS Colors
- 🟢 **Light Green**: Forward search frontier
- 🔴 **Light Red**: Backward search frontier
- 🟫 **Dark Green**: Forward visited cells
- 🟫 **Dark Red**: Backward visited cells

## Controls

- Right-click to open algorithm selection menu
- Choose from BFS, DFS, or Bidirectional BFS
- Watch the real-time visualization

## Requirements

- MinGW-w64 with g++
- OpenGL/GLUT libraries (freeglut, glew32)
- Windows (current implementation)

## Building

```bash
g++ -g main.cpp Cell.cpp -I. -L. -lfreeglut -lglew32 -lopengl32 -lglu32 -o main.exe
```

## Project Structure

```
Graphics/
├── Graphics/
│   ├── main.cpp          # Main application
│   ├── Cell.cpp          # Cell class implementation  
│   ├── Cell.h            # Cell class header
│   ├── freeglut.dll      # Required DLL
│   ├── glew32.dll        # Required DLL
│   └── *.lib             # Static libraries
└── Debug/                # Build outputs
```
