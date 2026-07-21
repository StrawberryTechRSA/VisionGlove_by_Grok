# Beginner exercise 02 — New gesture (Rock)

**Needs:** rebuild C++ (unlike config knobs).  
**No hardware.**  
**Goal:** Add a **new hand meaning** the software recognizes, prove it with a **test**, demo it with `--demo rock`.

---

## What is a “gesture” here?

Five flex values (0 = finger straight, 1 = finger bent):

| Index | Finger |
|-------|--------|
| 0 | Thumb |
| 1 | Index |
| 2 | Middle |
| 3 | Ring |
| 4 | Pinky |

A **gesture** is a pattern of open/closed fingers the code names (fist, point, peace, …).

---

## What we added: **Rock** (“horns”)

| Finger | State | Typical flex value |
|--------|--------|---------------------|
| Thumb | closed | ~0.85 |
| Index | **open** | ~0.1 |
| Middle | closed | ~0.9 |
| Ring | closed | ~0.9 |
| Pinky | **open** | ~0.1 |

**Name in code:** `Gesture::Rock` · string `"rock"`  
**Does it SMS?** No — not wired to Emergency (that stays panic fist / button). Rock is a **safe practice gesture**.

---

## Files you touch for any new gesture (map)

| Step | File | What |
|------|------|------|
| 1 | `include/visionglove/types.hpp` | Add enum value + `to_string` |
| 2 | `src/sensors.cpp` | `classify()` rule |
| 3 | `src/sensors.cpp` | optional `--demo` scenario inject |
| 4 | `src/glove_system.cpp` | self-test with fixed flex array |
| 5 | `src/main.cpp` | help text list (optional) |
| 6 | Rebuild | `build.bat` or g++ |
| 7 | Check | `--test` and `--demo rock` |

---

## Run what we shipped

```bat
cd C:\Users\nuraa\VisionGlove_by_Grok
.\build.bat
.\build\visionglove.exe --test
```

Look for:

```text
Test: PASS  gesture_rock
Self-test: PASSED (18/18)
```

(or 18+ tests including `gesture_rock`)

Demo:

```bat
.\build\visionglove.exe --demo rock --seconds 3
```

System should start cleanly (rock does not force Caution/Emergency by itself).

---

## Code that was added (so you can read it)

### 1) Enum — `types.hpp`

```cpp
Rock,            // index + pinky open ("horns")
```

### 2) Classifier — `sensors.cpp` → `classify`

```cpp
// Rock / horns: index + pinky open; thumb, middle, ring closed
if (flex[1] < oth && flex[4] < oth &&
    flex[0] > cth * 0.7 && flex[2] > cth && flex[3] > cth)
    return Gesture::Rock;
```

`oth` = open threshold (~0.30), `cth` = closed threshold (~0.70) from config.

### 3) Demo inject — `sensors.cpp` scenario `"rock"`

Forces flex injects for the five fingers as in the table above.

### 4) Test — `glove_system.cpp`

```cpp
std::array<double, 5> rock{{0.85, 0.1, 0.9, 0.9, 0.1}};
// update 5 times for hysteresis → expect Gesture::Rock
```

---

## Your turn later (exercise 02b — optional)

Add **your own** gesture the same way, e.g. **Phone** (thumb + pinky open, others closed — “call me”):

1. `Gesture::Phone` in `types.hpp`  
2. Rule in `classify`  
3. Test array e.g. `{0.1, 0.9, 0.9, 0.9, 0.1}`  
4. Scenario `"phone"`  
5. Rebuild + `--test`

---

## How this is “checked in reality” (no glove yet)

| Check | Command / action | Pass means |
|-------|------------------|------------|
| Unit test | `--test` → `gesture_rock` | Rule matches fixed numbers |
| Demo path | `--demo rock` | Inject path reaches the engine |
| Later hardware | Same flex pattern from serial/sensors | Real hand matches the array |

---

## Checklist

- [ ] Rebuild succeeded  
- [ ] `--test` shows `PASS  gesture_rock`  
- [ ] Full self-test PASSED  
- [ ] `--demo rock --seconds 3` runs without crash  
- [ ] You can point to the 4 file touch-points above  

**Next after this:** Beginner 03 — serial stub (feed flex numbers from outside).

---

*Strawberry Tech RSA · VisionGlove · Beginner 02*
