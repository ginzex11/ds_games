# Terrain Rules - Assignment Compliance

## ✅ Correct Implementation (Per Assignment)

### Movement Rules (isPassable)
```cpp
bool isPassable() const {
    return type == CellType::EMPTY || type == CellType::TREE || type == CellType::WAREHOUSE;
}
```

### Vision/Shooting Rules (blocksSight)
```cpp
bool blocksSight() const {
    return type == CellType::ROCK || type == CellType::TREE;
}
```

---

## Terrain Behavior Table

| Terrain Type | Can Move Through? | Can See Through? | Can Shoot Through? | Strategic Use |
|--------------|-------------------|------------------|-------------------|---------------|
| **Empty** | ✅ Yes | ✅ Yes | ✅ Yes | Open ground, no cover |
| **Rocks** | ❌ No | ❌ No | ❌ No | **Complete barrier, best hiding spot** |
| **Trees** | ✅ Yes | ❌ No | ❌ No | **Visual cover, can move through forest while hidden** |
| **Water** | ❌ No | ✅ Yes | ✅ Yes | **Terrain obstacle, but can shoot across** |
| **Warehouse** | ✅ Yes | ✅ Yes | ✅ Yes | Supply points, passable |

---

## Assignment Quote (Original Text)

> **Rocks.** Characters not can to pass the rocks. The shooting not penetrates rock. Also vision not penetrates rock. Place behind the rock is point hiding good.

Translation: Rocks block everything - movement, sight, shooting.

> **Trees.** The characters can to pass through the trees. Shooting or vision not pass trees.

Translation: Trees allow movement but block sight and shooting.

> **Water.** Characters not can to pass in water but shooting or vision pass in water.

Translation: Water blocks movement but allows sight and shooting.

---

## Tactical Implications

### Rocks 🪨
- **Complete Barrier**: Cannot move, see, or shoot through
- **Best Hiding Spot**: Perfect cover from enemy vision and fire
- **Strategic**: Use to block enemy paths and create safe zones

### Trees 🌲
- **Visual Cover**: Blocks enemy vision and shooting
- **Movement Allowed**: Can move through forests
- **Tactical Use**: 
  - Hide in forests while moving
  - Ambush enemies from tree cover
  - Retreat through forests for safety

### Water 💧
- **Movement Barrier**: Cannot cross water
- **Visual Transparency**: Can see enemies across water
- **Shooting Allowed**: Can shoot across water at visible enemies
- **Tactical Use**:
  - Natural team separator
  - Long-range engagement zones
  - Defensive positions at water edges

---

## Code Implementation

### File: `ai_simulation/include/common.h`

**Lines 154-161**:
```cpp
bool isPassable() const {
    // Per assignment: Trees allow movement, Rocks/Water block movement
    return type == CellType::EMPTY || type == CellType::TREE || type == CellType::WAREHOUSE;
}

bool blocksSight() const {
    // Per assignment: Rocks and Trees block sight/shooting, Water allows sight/shooting
    return type == CellType::ROCK || type == CellType::TREE;
}
```

---

## Gameplay Examples

### Example 1: Forest Ambush
```
Blue Warrior at (10, 10) - in open ground
Orange Warrior at (12, 10) - behind trees

Result: Blue CANNOT see Orange (trees block sight)
        Orange can move through trees to flank Blue
```

### Example 2: Water Engagement
```
Blue Warrior at (5, 15) - west side of water
Orange Warrior at (8, 15) - east side of water

Result: Both CAN see each other (water allows sight)
        Both CAN shoot at each other (water allows shooting)
        Neither can cross water to engage in melee
```

### Example 3: Rock Cover
```
Blue Warrior at (20, 20) - behind rock
Orange Warrior at (22, 20) - other side of rock

Result: Neither can see each other (rock blocks sight)
        Neither can shoot each other (rock blocks shooting)
        Rock provides perfect defensive position
```

---

## Testing Verification

### Test 1: Tree Movement ✅
```
1. Place warrior near tree
2. Path through tree cluster
3. Expected: Warrior walks through trees
```

### Test 2: Tree Vision ❌
```
1. Place Blue warrior at (10, 10)
2. Place Orange warrior at (12, 10) with tree at (11, 10)
3. Expected: Blue cannot see Orange (tree blocks)
```

### Test 3: Water Vision ✅
```
1. Place Blue warrior west of water
2. Place Orange warrior east of water
3. Expected: Both can see each other across water
```

### Test 4: Water Shooting ✅
```
1. Warriors on opposite sides of water
2. Both in range (< 8 tiles)
3. Expected: Both can shoot across water
```

### Test 5: Rock Barrier ❌
```
1. Place rock between two warriors
2. Expected: Neither can see or shoot through rock
```

---

## Assignment Compliance

✅ **Rocks**: Block movement, sight, shooting  
✅ **Trees**: Allow movement, block sight, block shooting  
✅ **Water**: Block movement, allow sight, allow shooting  

**Status**: 100% Compliant with Assignment Specification

---

**Build Status**: ✅ Compiled Successfully  
**Implementation**: Complete and Correct
