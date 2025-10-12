# AI Simulation Game - Team Combat Strategy

A C++ desktop simulation game demonstrating advanced AI behaviors for virtual characters in a team-based combat scenario on a grid-based map. Built with OpenGL and FreeGLUT for real-time graphical rendering.

## Overview

This simulation features two opposing teams (Blue and Orange) competing in tactical combat on a dynamic battlefield. Each team consists of 5 specialized units that work together using AI coordination, pathfinding, and strategic decision-making.

### Key Features

- **Team-Based Combat**: Two teams with distinct roles working together
- **Advanced AI**: A* pathfinding, BFS safe-position finding, visibility calculations
- **Strategic Gameplay**: Commanders coordinate team strategy while units act semi-autonomously
- **Dynamic Map**: Procedurally generated obstacles (rocks, trees, water) with resource warehouses
- **Real-time Rendering**: OpenGL visualization with character states and health bars

## Game Elements

### Teams

Each team has 5 characters:
- **1 Commander (C)**: Plans strategy, coordinates team, doesn't fight
- **2 Warriors (W)**: Combat specialists with limited ammo and grenades
- **1 Medic (M)**: Heals injured warriors
- **1 Supplier (P)**: Resupplies warriors with ammunition

**Blue Team** (left side) vs **Orange Team** (right side)

### Map Elements

- **Rocks** 🪨: Impassable, blocks sight and shots (dark gray)
- **Trees** 🌲: Passable, blocks sight and shots (green)
- **Water** 💧: Impassable, allows sight and shots (blue)
- **Warehouses** 🏪: Resource collection points (yellow)
  - Ammo warehouses for Suppliers
  - Medicine warehouses for Medics

### Victory Condition

Eliminate all enemy team members to win!

## AI Behaviors

### Commander (C)
- Collects visibility reports from all team members
- Builds comprehensive enemy position map
- Issues tactical orders (move, attack, defend, heal, resupply)
- Relocates to safer positions when threatened
- Non-combatant

### Warrior (W)
- Shoots enemies within range (requires ammo and line of sight)
- Throws grenades for area damage
- Follows commander's orders
- Requests resupply when ammo is low (≤5 bullets)
- Requests medical attention when injured (≤25% health)
- Acts independently if commander is eliminated

### Medic (M)
- Travels to medicine warehouse when needed
- Moves to injured warriors to heal them
- Heals warriors to full health
- Non-combatant

### Supplier (P)
- Travels to ammo warehouse when needed
- Resupplies warriors needing ammunition
- Provides 20 rounds per resupply
- Non-combatant

## Technical Implementation

### Architecture

```
ai_simulation/
├── CMakeLists.txt          # Build configuration
├── include/
│   ├── common.h            # Shared types, constants, enums
│   ├── map.h               # Grid map and obstacles
│   ├── ai.h                # A*, BFS, visibility algorithms
│   ├── character.h         # Base character class
│   ├── commander.h         # Commander specialization
│   ├── warrior.h           # Warrior specialization
│   ├── medic.h             # Medic specialization
│   ├── supplier.h          # Supplier specialization
│   └── simulation.h        # Game loop and rendering
├── src/
│   ├── main.cpp            # Entry point and window setup
│   ├── map.cpp
│   ├── ai.cpp
│   ├── character.cpp
│   ├── commander.cpp
│   ├── warrior.cpp
│   ├── medic.cpp
│   ├── supplier.cpp
│   └── simulation.cpp
└── test/
    └── tests.cpp           # Unit tests
```

### Core Algorithms

#### A* Pathfinding
- Finds optimal paths considering terrain and safety
- Safety weight factor to avoid enemy fire
- Handles dynamic obstacle avoidance

#### BFS Safe Position Finding
- Finds nearest safe position when under threat
- Considers enemy line of sight and range
- Used for defensive repositioning

#### Line of Sight (Bresenham's Algorithm)
- Ray-casting for visibility checks
- Considers terrain blocking (rocks, trees)
- Used for shooting and visibility calculations

#### Visibility Maps
- Circular range-based visibility
- Combined team awareness through commander
- Fog of war simulation

#### Safety Maps
- Heat map of dangerous areas
- Based on enemy positions and weapon ranges
- Influences pathfinding decisions

### Technologies

- **C++17**: Modern C++ features
- **OpenGL 4.6+**: Graphics rendering
- **FreeGLUT**: Window management and input
- **CMake 3.15+**: Build system

## Building and Running

### Prerequisites

- C++ compiler with C++17 support (g++, MSVC, clang++)
- CMake 3.15 or later
- OpenGL 4.6+
- FreeGLUT library

### Windows Build Instructions

```powershell
# Navigate to build directory
cd ai_simulation\build

# Configure CMake (adjust FreeGLUT paths if needed)
cmake ..

# Build
cmake --build . --config Release

# Run the simulation
.\AISimulationGame.exe

# Run tests
.\test_main.exe
```

### Linux Build Instructions

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install freeglut3-dev libglew-dev

# Navigate to build directory
cd ai_simulation/build

# Configure and build
cmake ..
make

# Run
./AISimulationGame

# Run tests
./test_main
```

## Controls

| Key | Action |
|-----|--------|
| **SPACE** | Pause/Resume simulation |
| **R** | Reset to initial state |
| **+** / **=** | Speed up simulation |
| **-** / **_** | Slow down simulation |
| **ESC** | Exit application |

## Game Configuration

Edit `include/common.h` to adjust game parameters:

```cpp
// Map dimensions
constexpr int GRID_WIDTH = 40;
constexpr int GRID_HEIGHT = 30;

// Combat parameters
constexpr int INITIAL_HEALTH = 100;
constexpr int INITIAL_AMMO = 30;
constexpr int WARRIOR_DAMAGE = 20;
constexpr int SHOOT_RANGE = 8;
constexpr int VISIBILITY_RANGE = 10;

// Thresholds
constexpr int LOW_HEALTH_THRESHOLD = 25;
constexpr int LOW_AMMO_THRESHOLD = 5;
```

## Design Principles

This project follows software engineering best practices:

- **SOC (Separation of Concerns)**: Modular architecture with distinct responsibilities
- **DRY (Don't Repeat Yourself)**: Reusable components and utilities
- **KISS (Keep It Simple)**: Clear, understandable implementations
- **TDD (Test-Driven Development)**: Comprehensive unit tests
- **YAGNI (You Aren't Gonna Need It)**: Only essential features implemented

## Testing

The project includes unit tests for:
- Map generation and queries
- A* pathfinding correctness
- Line of sight calculations
- Character behavior and state
- Safety map generation
- Utility functions

Run tests to verify core functionality:
```bash
./test_main
```

## Visual Guide

### Character Colors
- 🔵 **Blue Team**: Blue squares on the left
- 🟠 **Orange Team**: Orange squares on the right

### Character Letters
- **C**: Commander
- **W**: Warrior
- **M**: Medic
- **P**: Supplier (Provider)

### Health Bars
- Green bar below each character
- Length indicates remaining health

### Terrain Colors
- Light gray: Empty passable terrain
- Dark gray: Rocks (impassable)
- Green: Trees (passable but blocks sight)
- Blue: Water (impassable but allows sight)
- Yellow: Warehouses

## Future Enhancements

Potential improvements:
- Multiple maps with different layouts
- Configurable team compositions
- Enhanced AI strategies (flanking, ambush)
- Fog of war visualization
- Replay system
- Statistics and analytics
- Network multiplayer support

## License

This project is created for educational purposes demonstrating AI algorithms and game simulation techniques.

## Author

Created as a comprehensive demonstration of:
- AI pathfinding algorithms
- Team coordination systems
- Real-time strategy game mechanics
- OpenGL rendering techniques
- Modern C++ design patterns

---

**Note**: Adjust FreeGLUT library paths in `CMakeLists.txt` to match your system configuration.
