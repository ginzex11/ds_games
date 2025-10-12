# Quick Start Guide - AI Simulation Game

## 🚀 Get Started in 3 Steps

### Step 1: Build the Project
```bash
cd ai_simulation/build
cmake ..
cmake --build . --config Release
```

### Step 2: Run the Simulation
```bash
./AISimulationGame.exe
```

### Step 3: Watch the Battle!
- Blue team starts on the LEFT
- Orange team starts on the RIGHT
- Characters move and fight automatically

## 🎮 Controls

| Key | Action |
|-----|--------|
| `SPACE` | Pause/Resume |
| `R` | Reset |
| `+` | Speed up |
| `-` | Slow down |
| `ESC` | Exit |

## 📖 Reading the Screen

### Character Colors
- 🔵 **Blue squares** = Blue team
- 🟠 **Orange squares** = Orange team

### Character Letters
- **C** = Commander (leader, doesn't fight)
- **W** = Warrior (fighter with guns and grenades)
- **M** = Medic (healer)
- **P** = Supplier (gives ammo)

### Terrain Colors
- Light gray = Empty space (walkable)
- Dark gray = Rocks (can't walk or see through)
- Green = Trees (can walk but can't see through)
- Blue = Water (can't walk but CAN see through)
- Yellow = Warehouse (supply point)

### Health Bars
- Green bar under each character
- Longer bar = more health
- Short bar = low health, needs healing

## 🎯 What to Watch For

1. **Warriors** move toward enemies and shoot
2. **Commanders** stay back and coordinate
3. **Medics** run to heal injured warriors
4. **Suppliers** bring ammo to warriors
5. Characters use **smart pathfinding** to avoid obstacles
6. Teams work together to eliminate enemies

## 🏆 Victory

The first team to eliminate all enemy characters wins!

## 🧪 Testing

Run the tests to verify everything works:
```bash
./test_main.exe
```

You should see:
```
✓ All tests passed!
Passed: 19
Failed: 0
```

## 🔧 Troubleshooting

### "Missing DLL" error?
Make sure `libfreeglut.dll` is in the same folder as `AISimulationGame.exe`

### Window doesn't open?
- Check that OpenGL drivers are installed
- Try running from command line to see error messages

### Build fails?
- Make sure CMake 3.15+ is installed
- Check that g++ compiler is available
- Verify FreeGLUT library is installed

## 📚 Want to Learn More?

- Read `README.md` for detailed documentation
- Read `PROJECT_SUMMARY.md` for technical details
- Check the code comments for implementation details

## 🎉 That's It!

You're ready to watch AI teams battle it out!

Press SPACE to pause and observe strategies.
Press R to reset and watch a different battle.

Have fun! 🎮
