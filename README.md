# DS Games Portfolio
**Name**: Alexander Ginzburg  
**ID**: 208839613

## Projects

### 1. AI Simulation Game (Current)
**Location**: `ai_simulation/`  
**Status**: ✅ Complete (100% Assignment Compliance)

Advanced tactical AI simulation featuring:
- ✅ Water terrain (impassable, allows sight/shooting)
- ✅ Team visibility aggregation (commanders combine warrior vision)
- ✅ Combat system (shooting, grenades, health management)
- ✅ AI behaviors (autonomous combat, support seeking, team coordination)
- ✅ Advanced pathfinding (A*, BFS, safety maps)
- ✅ Rich visualization (fog of war, vision cones, weapon ranges)

**Quick Start**:
```bash
cd ai_simulation/build
cmake --build . --target AISimulationGame
./AISimulationGame.exe
```

**Documentation**:
- [Implementation Summary](ai_simulation/IMPLEMENTATION_SUMMARY.md) - Complete feature list and architecture
- [Assignment Checklist](ai_simulation/ASSIGNMENT_CHECKLIST.md) - Requirement verification and testing

**Latest Features**:
- **Team Visibility Aggregation**: Commanders see 60-68% more cells through warrior vision
  - Blue: 340 cells (213 own + 127 boost)
  - Orange: 272 cells (184 own + 88 boost)
- **Water Terrain**: Fully implemented as impassable terrain that allows sight/shooting

---

### 2. Pac-Man Game (Previous)
**Location**: `game_dev_ds/pacman_assignment/`

Classic Pac-Man implementation with OpenGL rendering.

**Build Commands**:
```bash
cd game_dev_ds/pacman_assignment
g++ -o Pacman.exe main.cpp Cell.cpp -lfreeglut -lopengl32 -lglu32 -lgdi32 -lwinmm -luser32 -I../third_party/freeglut/include -I. -L../third_party/freeglut/lib
./Pacman.exe
```

---

## Repository Structure
```
ds games/
├── ai_simulation/          # Current: AI tactical simulation
│   ├── include/            # Header files
│   ├── src/                # Source files
│   ├── build/              # Build output
│   ├── CMakeLists.txt      # CMake configuration
│   ├── IMPLEMENTATION_SUMMARY.md
│   └── ASSIGNMENT_CHECKLIST.md
│
├── game_dev_ds/            # Previous: Pac-Man game
│   └── pacman_assignment/
│
├── third_party/            # External libraries
│   └── freeglut/           # OpenGL utility library
│
└── README.md               # This file
```

---

## Technologies Used
- **C++17**: Modern C++ features
- **OpenGL**: Graphics rendering
- **FreeGLUT**: Window management and input handling
- **CMake**: Build system (AI simulation)
- **Git**: Version control

---

## Current Status
- ✅ AI Simulation Game: Complete and ready for submission
- ✅ All assignment requirements met (100%)
- ✅ Comprehensive documentation provided
- ✅ Tested and verified working

---

## Contact
Alexander Ginzburg  
ID: 208839613


