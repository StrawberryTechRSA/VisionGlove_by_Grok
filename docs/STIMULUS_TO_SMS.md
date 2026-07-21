# VisionGlove: From stimulus on the glove to SMS

**What this document is:** A plain walkthrough of what happens inside VisionGlove  
from the moment the world “touches” the glove until an emergency SMS is attempted.

**Mapped to the three non-roastable stages:**

| Stage | Name | When it runs |
|-------|------|----------------|
| **A** | Spec & tests (the answer key) | Mostly **before** runtime (build/CI), and as fixed rules the code obeys |
| **B** | Portable C++ core | **Every** live run on laptop or edge |
| **C** | Target kernels & binary | How Stage B is **built and run** on a chosen machine (today: normal optimized C++; later: SIMD/edge) |

**Reference code:** [VisionGlove_by_Grok](https://github.com/StrawberryTechRSA/VisionGlove_by_Grok)  
**Threat levels:** 0 Safe → 1 Caution → 2 Alert → 3 Emergency  

---

## Big picture (one breath)

```
Hand moves / camera sees people
        ↓
Sensors + vision sample the world          ← Stage B threads
        ↓
Fusion decides a threat level              ← Stage B (rules from Stage A)
        ↓
If level went up enough → haptics + dispatch
        ↓
At Alert+: build text → SMS service        ← Stage B
        ↓
Dry-run log  OR  (later) real carrier      ← Stage C = where the binary runs
```

---

## Stage A — Spec & tests (not a “sensor step”)

Stage A does **not** sit between the glove and the SMS as a live process.

It answers: **“What *should* happen for a given stimulus?”**

### What Stage A defines (examples)

| Stimulus (idea) | Expected outcome (answer key) |
|-----------------|-------------------------------|
| Five fingers strongly bent | Gesture = fist |
| Fist three times quickly | Panic sequence → Emergency |
| Violent acceleration | “Unusual movement” → at least Alert |
| Person count ≥ config threshold | At least Caution |
| Threat score ≥ 0.75 | Emergency |

### Where that lives today

- Self-tests (`visionglove --test`): gesture, threat, crypto, ring buffer  
- Config rules: `person_threshold`, `closed_threshold`, dry-run flags  
- Demo scenarios: `fist`, `panic`, `shake`, … used to force known stimuli  

### Role on the path to SMS

Stage A is the **contract**. Stage B is written to obey it.  
If Stage B ever sends SMS on “open hand, alone, calm,” Stage A tests should **fail** before you ship.

Think of Stage A as: **the rulebook + the exam.**  
The glove in the field only runs Stage B (and C’s binary).

---

## Stage B — What happens at runtime (stimulus → SMS)

This is the real pipeline inside the running program.

### Step 0 — System is already up

On start, VisionGlove:

1. Loads **JSON config** (contacts, thresholds, `dry_run`, camera/sensor flags).  
2. Starts **auth** (session key for signing events).  
3. Starts **sensor thread** (~100 Hz target).  
4. Starts **vision thread** (~30 FPS target).  
5. Starts **fusion loop** (~50 Hz, every ~20 ms).  
6. Arms **haptics** and **emergency dispatcher** (SMS + livestream services).

Until something interesting happens, threat stays **Safe (0)** and **no SMS** is sent.

---

### Step 1 — Stimulus arrives at the glove

“Stimulus” means anything the hardware (or simulator) presents as input.

| Stimulus type | Physical meaning | Software entry |
|---------------|------------------|----------------|
| Finger bend | Flex sensors on fingers | Flex readings 0…1 |
| Motion / orientation | IMU (accel, gyro, mag) | Calibrated vectors + orientation |
| Grip / pressure | Palm / fingertip FSRs | Pressure 0…1 |
| Scene / people | Camera (or simulated count) | Person detections + count |

**Today:** sensors can be **simulated** or **injected** (demo scenarios).  
**Later:** same Stage B APIs, real ADC/I2C/serial behind them (still Stage B code).

---

### Step 2 — Sensor thread turns raw signals into meaning

**Where:** `SensorManager` on its own thread (Stage B).  
**How often:** target ~100 times per second.

For each cycle:

1. **Read** flex ×5, IMU, pressure ×5 (sim, inject, or hardware).  
2. **Filter** (e.g. EMA on flex) so noise does not spam gestures.  
3. **Classify gesture** (open hand, fist, point, peace, thumbs up, …) with **hysteresis** so the label does not flicker every sample.  
4. **Panic detection:** e.g. several fists inside a short time window → `PanicSequence` / `emergency_gesture = true`.  
5. **Unusual movement:** if acceleration magnitude exceeds threshold → flag set.  
6. Pack everything into a **`SensorSnapshot`**.  
7. Push into an **SPSC ring buffer** (single-producer, single-consumer).  
   - If fusion is slow and the queue is full → **drop oldest**, keep freshest.  
   - Prefer live truth over a backlog of stale fists.

**Stage C note:** Later, the hottest math here (filters, norms) might get SIMD on one chip.  
**Today:** plain portable C++ — still Stage B logic.

---

### Step 3 — Vision thread samples the scene (in parallel)

**Where:** `VisionProcessor` on its own thread (Stage B).  
**How often:** target ~30 FPS.

For each frame:

1. Capture frame **or** use simulated person count.  
2. **Person detector** → count + simple boxes.  
3. Read the **latest sensor context** fusion shared in (so vision threat can use motion/panic too).  
4. **ThreatAnalyzer** scores continuous risk 0…1 from:  
   - person count / crowd  
   - unusual movement  
   - emergency / panic gesture  
   - grip + fist, etc.  
5. Map score → vision-side threat hint (Safe / Caution / Alert / Emergency).  
6. Pack **`VisionSnapshot`** → another **SPSC** queue for fusion.

Vision does **not** send SMS. It only proposes how scary the *scene* looks.

---

### Step 4 — Fusion loop decides the system threat level

**Where:** `GloveSystem::main_loop` (Stage B), ~every 20 ms.

Each tick:

1. **Pop latest** sensor snapshot (if any).  
2. Feed that snapshot to vision as context.  
3. **Pop latest** vision snapshot (if any).  
4. **`fuse(sensor, vision)`** — single authority for the glove’s threat level:

```
IF emergency_gesture OR panic sequence
    → Emergency (hard override)

ELSE start from Safe, then raise only:
    vision threat hint
    unusual_movement        → at least Alert
    person_count ≥ threshold → at least Caution
    vision threat_score ≥ 0.75 → Emergency
    vision threat_score ≥ 0.45 → Alert
```

5. Compare to **previous** threat level.  
   - **Unchanged** → do nothing (no SMS spam every 20 ms).  
   - **Changed** → go to Step 5.

**This is the brain of the glove.** Stages A’s rules are encoded here and in the analyzer/gesture code.

---

### Step 5 — Threat changed: haptics + event integrity

**Where:** `GloveSystem::on_threat_change` (Stage B).

When level changes (e.g. Safe → Alert, or Alert → Emergency):

1. **Log** the transition.  
2. **Haptics:** pattern by level  
   - Caution → gentle pulse  
   - Alert → rapid pulse  
   - Emergency → continuous buzz  
3. **If new level ≥ Alert** → call emergency **dispatch** (Step 6).  
   - Caution alone does **not** SMS in current code; it primes/logs.  
4. **Sign** the transition with **HMAC-SHA256** (auth session key) and log a short MAC fingerprint so the event is harder to silently fake in logs.

Wearer feels the buzz **before or as** messaging runs—same handler, same moment.

---

### Step 6 — Emergency dispatcher chooses actions by level

**Where:** `EmergencyDispatcher::dispatch` (Stage B).

Creates an event id (e.g. `EMG-<timestamp>`), then runs **stacked** handlers:

| Level reached | Handler | What it does |
|---------------|---------|----------------|
| ≥ Caution (1) | `handle_caution` | Log / prime systems only |
| ≥ Alert (2) | `handle_alert` | **SMS to `emergency_contact`** |
| ≥ Emergency (3) | `handle_emergency` | SMS to `police_number` (if set) + start livestream |

So for a jump **straight to Emergency**, the code still runs caution + alert + emergency handlers: contact SMS **and** police/stream paths as configured.

---

### Step 7 — SMS is “sent”

**Where:** `SmsService::send_sms` (Stage B).

1. Build message text, e.g.  
   `VisionGlove ALERT at Unknown (id=EMG-…)`  
   or emergency wording for police line.  
2. If **`emergency_contact`** (or police number) is empty → skip, log why.  
3. If **`dry_run: true`** (default in config):  
   - **Do not** call Twilio/carrier.  
   - Log: `[dry-run] to=+… body=…`  
   - Treat as success for demos and tests.  
4. If dry-run is off **and** credentials exist:  
   - **Intended** live path (HTTP to provider).  
   - Current open-source build **refuses silent fake success** without a real HTTP client wired—so production SMS is a deliberate integration step, not a stub that pretends.

**User-facing meaning of “SMS sent” in demos:**  
message was accepted by the SMS service layer and logged (dry-run) or handed to the provider (live).

---

## Stage C — Where the binary runs (and later, how fast)

Stage C is **not** another if/else between fusion and SMS.

| Stage C topic | Meaning on the stimulus→SMS path |
|---------------|-----------------------------------|
| **Compilation** | Stage B C++ → machine code for **one** target (PC today; MCU/SoC later) |
| **Timing** | Same steps, but measured p99 (e.g. fusion &lt; 5 ms on edge target) |
| **Optional kernels** | Only if profiling says flex/IMU math is hot—replace *inner loops*, not the SMS story |
| **Deployment** | Edge gadget runs the **same Stage B pipeline** as a pure binary |

So:

- **Stimulus → decision → SMS logic** = Stage B (always).  
- **Running fast enough on the glove hardware** = Stage C.  
- **Knowing the decision was correct** = Stage A.

---

## End-to-end example: panic fist on a crowded street

| Time (approx) | Stage | What happens |
|---------------|-------|----------------|
| t = 0 | B | Wearer clenches fist hard, thrice; IMU jolts; camera sees 4 people |
| t &lt; 10 ms | B | Sensor thread reads flex/IMU; gesture engine sees fist edges; panic window fills → `emergency_gesture` |
| same era | B | Vision thread: person_count = 4 ≥ threshold; score rises with panic + crowd |
| ≤ 20 ms later | B | Fusion: hard override → **Emergency** |
| same tick | B | Haptics: continuous buzz; HMAC log of transition |
| same tick | B | Dispatch: caution log + **SMS to emergency_contact** + police/stream if configured |
| same call | B/C | SMS layer dry-runs or transmits; process runs as Stage C binary on host |

**SMS that actually leaves the phone/network** only if dry-run is false, numbers are set, and a live provider is wired.  
**SMS that “fires” in product logic** happens at Alert+ via `handle_alert` / emergency handlers.

---

## What does *not* happen (important)

- Stage A does not receive glove samples in the field.  
- Vision does not text people by itself.  
- Fusion does not spam SMS every frame—only on **threat level change**.  
- Caution alone does not SMS in the current Grok implementation.  
- “Pure binary” is not a separate messaging stack; it is Stage B compiled.

---

## One-sentence version

**The glove samples hand and scene (Stage B), fuses them into a threat level using rules proven by tests (Stage A), and only when that level rises to Alert or higher does it fire haptics and the SMS dispatcher—while Stage C is simply that whole core running as a measured binary on the chosen hardware.**

---

## Related docs

- [ARCHITECTURE_ONE_PAGER.md](./ARCHITECTURE_ONE_PAGER.md) — stages, contracts, SLOs  
- [COMPARISON.md](../COMPARISON.md) — Claude vs Grok  
- Config: `config/config.json` — contacts, thresholds, `dry_run`

---

*Strawberry Tech RSA · VisionGlove · Stimulus-to-SMS walkthrough*
