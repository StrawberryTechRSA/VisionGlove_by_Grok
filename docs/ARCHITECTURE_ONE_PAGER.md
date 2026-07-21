# VisionGlove — Architecture One-Pager (Strawberry Tech RSA)

**Product:** Real-time cybernetic safety glove (sensors → fusion → threat → haptics / emergency)  
**Reference impl:** [VisionGlove_by_Grok](https://github.com/StrawberryTechRSA/VisionGlove_by_Grok) (C++20)  
**Status:** Working simulation + self-tests; hardware bridge is the next contract  
**Last updated:** 2026-07-21

---

## 1. Problem & non-goals

| In scope | Out of scope (for now) |
|----------|-------------------------|
| Subsystem fusion at fixed rates | Training large CV models on-device |
| Deterministic hotpath latency | Full SAS/R analytics stack on edge |
| Portable C++ core + optional SIMD | Hand-written AVX-512 as default path |
| Dry-run emergency actions by default | Unattended police SMS without policy |

**Primary SLO (simulation / laptop today):** fusion loop ≤ 20 ms (50 Hz); sensor path target 10 ms (100 Hz).  
**Primary SLO (edge target):** threat decision hotpath **p99 < 5 ms** on chosen MCU/SoC (TBD hardware).

---

## 2. System context

```
[Flex ×5] [IMU] [Pressure ×5] [Camera optional]
        \       |        /           |
         \      |       /            v
          v     v      v      [Vision thread 30 FPS]
        [Sensor thread ~100 Hz]
                 \           /
                  v         v
              [SPSC queues — drop-oldest]
                       |
                       v
              [Fusion loop ~50 Hz]
                 /    |     \
                v     v      v
           Haptics  Threat  Emergency (dry-run)
                       |
                       v
              HMAC-signed event log
```

**Source of truth for math:** unit tests + golden scenarios (`idle`, `fist`, `panic`, …).  
**Source of truth for production binary:** C++ core after port; Python/R only as oracles, never as the edge runtime.

---

## 3. Three stages (not five slogans)

### Stage A — Spec & oracle

| | |
|--|--|
| **Purpose** | Prove the *algorithm*, not the machine |
| **Where** | Workstation / CI |
| **Tools** | Python *or* C++ tests (one reference path); fixed datasets |
| **Inputs** | Synthetic or logged sensor traces (CSV/binary) |
| **Outputs** | Expected gestures, threat levels, pass/fail vectors |
| **Owner** | Algorithms / product |
| **Metric** | 100% of golden cases green; no silent float drift beyond ε |
| **Kill criterion** | If oracle and C++ disagree on goldens → **block release** |

*Do not* list SAS/R/Viya as runtime tools unless a paying workflow requires them offline. They are optional *analysis*, not Stage A of the glove.

---

### Stage B — Portable core (architectural skeleton)

| | |
|--|--|
| **Purpose** | Correct, maintainable real-time system |
| **Where** | IDE / CI; same code on laptop and edge (with shims) |
| **Tools** | **Modern C++20**, CMake/MSYS2 build; no required intrinsics |
| **Layout** | **SoA** (or SoA-friendly arrays) for flex, pressure, IMU history rings |
| **Concurrency** | Sensor thread, vision thread, fusion thread; **SPSC lock-free rings** |
| **Inputs** | `SensorSnapshot`, `VisionSnapshot` (typed contracts in `types.hpp`) |
| **Outputs** | `ThreatLevel`, haptic command, emergency dispatch request |
| **Owner** | Platform / firmware-adjacent |
| **Metric** | Self-test suite green (crypto, gestures, threat, ring buffer); measured Hz/FPS |
| **Kill criterion** | Data race, unbounded queue growth, or threat false-negative on panic golden |

**SoA contract (example):**

```
flex_raw[N][5], flex_filt[N][5]     // N = ring capacity, 5 fingers
accel[N][3], gyro[N][3]
pressure[N][5]
```

Hot loops iterate **time then channel** or **channel then time** deliberately—document the order next to the arrays.

---

### Stage C — Target kernels & production binary

| | |
|--|--|
| **Purpose** | Meet latency/power on **one** primary target |
| **Where** | Chosen edge SoC / MCU + optional host accelerator |
| **Tools** | Optimized C++ first; **SIMD intrinsics only where Stage B profile proves need** |
| **Default vector path** | Compiler auto-vectorization + `-O2/-O3`; AVX2 *or* NEON as first manual step |
| **AVX-512 / GPU / NPU** | Optional backends behind `#if` / runtime feature detect — **not** the happy path |
| **Inputs** | Same Stage B snapshots (ABI-stable where possible) |
| **Outputs** | Same threat/haptic/emergency commands; binary artifact + version stamp |
| **Owner** | Performance / embedded |
| **Metric** | p50/p99 fusion latency; mJ/decision if battery; binary size; thermal steady-state |
| **Kill criterion** | p99 miss SLO, or build requires hardware only one person owns |

**Assembly (`vaddps` / `vmulps`):** inspection and profiling artifact (Godbolt / `perf` / VTune), **not** a product stage. If you hand-write asm, it lives as a *named kernel* with a scalar fallback and a test.

---

## 4. Data contracts between stages

| From → To | Contract |
|-----------|----------|
| A → B | Golden file format + version; ε for floats; threat enum 0–3 |
| B → C | Same `SensorSnapshot` / `VisionSnapshot` fields; no Python objects |
| C → field | Config JSON (no secrets in repo); dry_run flags; signed event log |

**Threat levels (stable enum):**

| Level | Name | Typical triggers | Actions |
|-------|------|------------------|---------|
| 0 | Safe | Nominal | None |
| 1 | Caution | Crowd ≥ person_threshold | Log, gentle haptic |
| 2 | Alert | Unusual motion, elevated score | SMS contact (if configured), rapid haptic |
| 3 | Emergency | Panic gesture / score ≥ 0.75 | Full dispatch + livestream dry-run policy |

---

## 5. Threading & latency budget (current Grok design)

| Path | Target rate | Budget | Notes |
|------|-------------|--------|-------|
| Sensor collect + process | 100 Hz | 10 ms | Drop-oldest if fusion lags |
| Vision (sim or camera) | 30 FPS | ~33 ms | Multi-modal score uses last sensor ctx |
| Fusion + threat + side effects | 50 Hz | 20 ms | Side effects must not block; queue work |
| Emergency I/O | async / dry-run | N/A | Never block fusion on network |

**Overload policy:** SPSC full → drop oldest snapshot (prefer fresh state over backlog).

---

## 6. Security & safety contracts

| Rule | Why |
|------|-----|
| Emergency SMS/stream **dry-run default** | No accidental real dispatch in demos |
| HMAC-SHA256 on event payloads | Integrity of logs / transitions |
| No secrets in git | Config placeholders only |
| Constant-time verify where applicable | Avoid trivial MAC bypass |

---

## 7. What we deliberately collapsed from the 5-frame deck

| Old “frame” | New home |
|-------------|----------|
| Python / R / SAS / AI POC | Stage A (oracle only; one reference) |
| C++ SoA | Stage B (required) |
| Intrinsics / 256 & 512-bit | Stage C (optional, profile-gated) |
| Native asm listing | Profiling / review, not a stage |
| Pure binary on edge | Stage C artifact + deploy checklist |

---

## 8. Near-term roadmap (contracts, not vibes)

1. **Freeze golden set** — 20+ traces covering open/fist/point/panic/shake + multi-person vision sim.  
2. **Hardware shim** — serial/I2C adapter implementing `inject_*` / read APIs; Stage B unchanged.  
3. **Pick primary edge** — e.g. ESP32-S3 *or* RPi Zero 2 *or* phone companion; document once.  
4. **Profile Stage B** on that target; only then add NEON/AVX kernels for top 1–2 hotspots.  
5. **Policy pack** — legal/safety copy for emergency contacts; still dry-run until signed off.

---

## 9. Definition of done (release checklist)

- [ ] Stage A goldens pass on CI  
- [ ] Stage B `visionglove --test` all green  
- [ ] Measured sensor Hz / vision FPS logged in CI or release notes  
- [ ] Config validates; dry_run true in default config  
- [ ] Threat transitions signed; panic golden → Emergency  
- [ ] Stage C target named; p99 latency recorded  
- [ ] No unbounded queues; stop() joins all threads  

---

## 10. One-sentence architecture

**VisionGlove is a fixed-rate, multi-threaded C++ fusion system with SoA sensor buffers and SPSC handoff, proven by golden oracles, shipped as a portable core, and sped up only where profiles demand—on one chosen edge target with dry-run-safe emergency side effects.**

---

*Strawberry Tech RSA · VisionGlove · Architecture one-pager*
