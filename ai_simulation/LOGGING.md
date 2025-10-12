# AI Simulation - Logging System

## Overview
The game now uses a file-based logging system that writes all events to separate log files for easy analysis after each simulation run.

## Log Files

### Location
All logs are written to the `build/logs/` directory:
- `character_log.txt` - All character actions and AI decisions
- `control_log.txt` - User input and system events

### Behavior
- Log files are **cleared at the start of each run** (not appended)
- Each file has a timestamped header showing when the simulation started
- Logs are written in real-time (flushed after each entry)
- If the game crashes, all logs up to that point are preserved

## Log Contents

### Character Log (character_log.txt)
Records all character behavior and AI decisions:
- **Commander**: Order distribution, team management
- **Warrior**: Status (HP, ammo), combat actions, path blocking
- **Medic**: Healing operations, warehouse trips, patient status
- **Supplier**: Resupply operations, warehouse trips, recipient status

Example:
```
[COMMANDER Blue] Issuing orders to team...
  - Issuing ATTACK order to W at (7,13)
[WARRIOR Blue at (7,13)] HP:85/100 | Ammo:45/50 | Order:ATTACK
[MEDIC Blue] Executing HEAL order for Blue warrior at (7,13)
[MEDIC Blue] Distance to patient: 1 | Has medicine: Yes | Supplies: 5
[MEDIC Blue] Healing patient!
```

### Control Log (control_log.txt)
Records system events and user input:
- Game initialization (window, OpenGL)
- Keyboard inputs with keys pressed
- Feature toggles (fog of war, debug mode)

Example:
```
=== Simulation Starting ===
Window created successfully
OpenGL initialized
Key pressed: 102 ('f')
Toggling fog of war
```

## Usage

### For Developers
Use the logging macros in any source file:
```cpp
#include "common.h"  // Includes Logger.h and macros

// Log character actions
LOG_CHARACTER("Warrior " << team << " attacking at " << pos);

// Log control events
LOG_CONTROL("User toggled feature X");
```

### For Analysis
After running a simulation:
1. Open `build/logs/character_log.txt` to analyze character behavior
2. Open `build/logs/control_log.txt` to see what user inputs occurred
3. Use text search to find specific events (e.g., search "MEDIC" or "HEAL")
4. Compare timestamps to understand event sequences

## Technical Details

### Implementation
- **Logger Class**: Singleton with static methods (`Logger::log()`)
- **Thread Safety**: Uses `std::mutex` for concurrent access
- **File Mode**: Opens files in truncate mode (`std::ios::trunc`)
- **Directory Creation**: Automatically creates `logs/` using `std::filesystem`
- **Macros**: Stream-style syntax using `std::ostringstream`

### Files
- `include/Logger.h` - Logger class declaration
- `src/Logger.cpp` - Logger implementation
- `include/common.h` - Logging macros (LOG_CHARACTER, LOG_CONTROL)

### Initialization
Logger is initialized in `main.cpp`:
```cpp
Logger::initialize("logs");  // At startup
Logger::shutdown();          // On exit
```

## Troubleshooting

### Logs Not Created
- Ensure write permissions in the `build/` directory
- Check that `logs/` directory was created
- Logger must be initialized before any LOG_* calls

### Missing Character Logs
- Verify all character source files use LOG_CHARACTER macro
- Check that character is actually performing actions
- Ensure Logger is initialized before simulation starts

### Performance
- Logging adds minimal overhead (uses buffered I/O)
- Files are flushed after each log for crash safety
- For performance-critical sections, reduce logging frequency

## Future Enhancements
Potential improvements:
- Configurable log levels (DEBUG, INFO, WARNING, ERROR)
- Log rotation for long-running simulations
- JSON or structured logging format
- Real-time log viewer UI
- Performance profiling logs
