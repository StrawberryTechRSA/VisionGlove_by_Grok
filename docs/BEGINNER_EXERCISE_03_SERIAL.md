# Beginner exercise 03 — Serial stub (hardware bridge without a glove)

**Needs:** rebuild.  
**Hardware:** **not required** — we use a **text file** that pretends to be a serial stream.  
**Goal:** Feed the **same Stage B sense path** from outside the built-in sim.

---

## What is a serial stub?

Real gadgets do this:

```text
MCU / sensors  --bytes-->  PC / app  --parse-->  numbers  --same brain-->  fuse / SMS
```

A **stub** is a stand-in for the MCU:

```text
sample_serial_feed.txt  --lines-->  SerialStub  --inject-->  flex/IMU  -->  fuse
```

Later you only replace the **source** (COM port from Arduino) — not `fuse()`.

---

## Line protocol (simple on purpose)

| Line | Meaning |
|------|---------|
| `FLEX,t,i,m,r,p` | Five flex values 0.0–1.0 (thumb…pinky), 0=open, 1=bent |
| `VG,t,i,m,r,p` | Same as FLEX |
| `IMU,ax,ay,az,gx,gy,gz` | Accel m/s², gyro rad/s |
| `# comment` | Ignored |

Example rock hand:

```text
FLEX,0.85,0.10,0.90,0.90,0.10
```

---

## What we added in the repo

| Piece | Role |
|-------|------|
| `serial_stub.hpp` / `serial_stub.cpp` | Parse lines + read looping file |
| `SensorManager` scenario `serial` | Pump feed → inject flex/IMU |
| `--serial-file path` | CLI attach feed |
| `--demo serial` | Uses `config/sample_serial_feed.txt` |
| Self-tests | `serial_parse_flex`, `serial_parse_imu`, `serial_parse_comment` |
| `config/sample_serial_feed.txt` | open → point → rock → fist → open |

---

## Run it

```bat
cd C:\Users\nuraa\VisionGlove_by_Grok
.\build.bat
.\build\visionglove.exe --test
```

Expect new PASS lines:

```text
Test: PASS  serial_parse_flex
Test: PASS  serial_parse_imu
Test: PASS  serial_parse_comment
Self-test: PASSED (21/21)
```

(count may be 21 if Rock tests are included)

### Demo feed (file = fake serial)

```bat
.\build\visionglove.exe --demo serial --seconds 5 --debug
```

or:

```bat
.\build\visionglove.exe --serial-file config\sample_serial_feed.txt --seconds 5
```

You should see the feed open, sensors run, and possibly threat changes when fist lines appear (depending on timing/hysteresis).

### Edit the feed yourself

1. Open `config\sample_serial_feed.txt`  
2. Add a line, e.g. only rock forever:

```text
FLEX,0.85,0.10,0.90,0.90,0.10
```

3. Save, re-run `--demo serial`  
4. No C++ change required for new **numbers** — only for new **packet types**

---

## How this checks “hardware reality” later

| Layer | Now | With real MCU |
|-------|-----|----------------|
| Parse | `--test` serial_parse_* | Same parser |
| Pipeline | File loops into inject | MCU prints `FLEX,...\n` on USB serial |
| Golden move | Edit file to known fist | Do fist on desk; serial shows 0.9s |
| Pass | Gesture/threat match rules | Same |

**Reality checklist for a real board:**

1. MCU prints the **same** line format  
2. PC opens COM port (future: `--serial-port COM3`) — today file proves the parser + inject  
3. Known move → known gesture in logs  
4. `--test` still green  

---

## Files to read

| File | Why |
|------|-----|
| `src/serial_stub.cpp` | `parse_sensor_line` |
| `src/sensors.cpp` | `scenario == "serial"` |
| `config/sample_serial_feed.txt` | Your fake glove |
| `src/main.cpp` | `--serial-file` |

---

## Checklist

- [ ] Rebuild + `--test` includes serial_parse_* PASS  
- [ ] `--demo serial --seconds 5` runs  
- [ ] You changed one FLEX line in the sample file and re-ran  
- [ ] You can explain: **file today = COM port tomorrow**  

---

## Order complete

| # | Exercise | Skill |
|---|----------|--------|
| 1 | Config knobs | Tune without C++ |
| 2 | Rock gesture | Product logic + test |
| 3 | Serial stub | Hardware bridge thinking |

**Next (optional hardware):** Arduino/ESP32 `Serial.println("FLEX,0.9,0.9,0.9,0.9,0.9");` and a small COM reader (can be added when you have a board).

---

*Strawberry Tech RSA · VisionGlove · Beginner 03*
