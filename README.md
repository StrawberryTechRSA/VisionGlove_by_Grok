# VisionGlove by Grok (C++20)

A **real-time cybernetic safety glove** stack rewritten in modern C++ to outperform the Claude Python prototype at [VisionGlove_by_Claude](https://github.com/StrawberryTechRSA/VisionGlove_by_Claude).

| | Claude (Python) | Grok (C++20) |
|--|-----------------|--------------|
| Language | Python asyncio | C++20 multi-threaded |
| Sensors | `sin()` stubs | EMA filtering, calibration, scenario injection |
| IMU | Broken Euler stubs | Quaternion complementary filter |
| Gestures | Threshold ifs | Hysteresis FSM + panic sequence detector |
| Threat | Always returns 0 | Multi-factor weighted scoring |
| Concurrency | Unbounded lists | SPSC lock-free ring buffers |
| Security | `token_hex` unused | Real SHA-256 + HMAC-SHA256, constant-time verify |
| Package | Broken `vision_glove.*` imports | Builds and runs |
| Self-test | Structural only | Cryptographic + algorithmic asserts |

## Build (Windows + MSYS2)

```powershell
cd C:\Users\nuraa\VisionGlove_by_Grok
.\build.ps1
```

Or with CMake (if installed):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```powershell
# Unit / integration self-tests
.\build\visionglove.exe --test

# Live simulation (5 seconds)
.\build\visionglove.exe --demo panic --persons 4 --seconds 5

# Fist scenario with debug logs
.\build\visionglove.exe --demo fist --debug --seconds 3
```

### CLI

| Flag | Meaning |
|------|---------|
| `--test` | Self-test and exit |
| `--config path` | JSON config |
| `--demo name` | `idle` `fist` `open` `point` `shake` `panic` |
| `--persons N` | Simulated people in view |
| `--seconds N` | Auto-exit after N seconds |
| `--debug` | Verbose status |

## Architecture

```
Sensor thread (100 Hz) ──SPSC──► Fusion loop (50 Hz) ◄──SPSC── Vision thread (30 FPS)
                                      │
                                      ├─► Haptics patterns
                                      ├─► Emergency dispatcher (dry-run SMS/stream)
                                      └─► HMAC-signed event log
```

## Safety note

Emergency SMS/livestream default to **dry-run**. No real messages are sent unless you deliberately configure credentials and disable dry-run.

## License

Apache-2.0 (same spirit as the Claude port of VisionGlove).
