# VisionGlove: Claude Python vs Grok C++20

Source of Claude port: https://github.com/StrawberryTechRSA/VisionGlove_by_Claude

## Scorecard

| Capability | Claude (Python) | Grok (C++20) | Winner |
|------------|-----------------|--------------|--------|
| Builds & runs | Broken package imports (`vision_glove.core` missing) | Compiles with g++ 14, `--test` 17/17 | **Grok** |
| Sensor filtering | Raw `sin()` + hash noise | EMA denoise + min/max calibration | **Grok** |
| IMU fusion | Stub Euler, incomplete accel integration | Quaternion complementary filter (gyro + accel tilt) | **Grok** |
| Gesture recognition | Simple thresholds | Hysteresis FSM + triple-fist panic window | **Grok** |
| Threat analysis | Returns constant level 0 | Weighted multi-factor score → 4 levels | **Grok** |
| Real-time architecture | asyncio + unbounded history lists | Dedicated threads + SPSC lock-free rings | **Grok** |
| Main loop rate | 10 Hz | 50 Hz fusion, 100 Hz sensors, 30 FPS vision | **Grok** |
| Security | Unused `secrets.token_hex` | SHA-256 + HMAC-SHA256, constant-time verify | **Grok** |
| Emergency dry-run | Soft stubs | Explicit dry-run, signed events, action log | **Grok** |
| Self-tests | Config shape only | Crypto vectors, gestures, ring buffer, threat | **Grok** |
| Zero-dependency deploy | Heavy TF/torch/mediapipe list | Stdlib-only binary ~1.3 MB | **Grok** |

## Architectural differences that matter

### Claude
- Looks “production-ready” in the README (`docs_by_Claude`).
- Person detector, gesture recognizer, threat analyzer are **empty stubs**.
- Haptics is `asyncio.sleep(0.1)`.
- Auth generates a key and never uses it.
- `main_app.py` imports a package layout that does not exist in the repo.

### Grok
- Same product surface (sensors → vision → threat → haptics → emergency).
- Algorithms are **testable and tested**.
- Deterministic latency path via fixed-capacity SPSC queues (drop-oldest under overload).
- Safe defaults: SMS/stream dry-run unless configured.

## How to reproduce

```powershell
cd C:\Users\nuraa\VisionGlove_by_Grok
# via MSYS2:
# export PATH=/ucrt64/bin:$PATH
# g++ -std=c++20 -O2 -I include -o build/visionglove.exe src/*.cpp
.\build\visionglove.exe --test
.\build\visionglove.exe --demo panic --persons 4 --seconds 5 --debug
```

## Bottom line

Claude delivered a **polished folder structure and marketing docs**.  
Grok delivered a **working real-time C++ system** with real fusion, threat scoring, crypto, and green self-tests.
