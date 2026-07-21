# VisionGlove sensing policy  
### Based on real gadgets · What’s standard vs what we added

**Product:** VisionGlove (Strawberry Tech RSA)  
**Code:** [VisionGlove_by_Grok](https://github.com/StrawberryTechRSA/VisionGlove_by_Grok)  
**Related:** [STIMULUS_TO_SMS.md](./STIMULUS_TO_SMS.md) · [ARCHITECTURE_ONE_PAGER.md](./ARCHITECTURE_ONE_PAGER.md)

---

## 1. How real gadgets sense (not marketing — product patterns)

Safety and wearables almost always use a **fast cheap sensor** first, and **camera / cloud** second (or never for the first SMS).

| Real gadget / class | Primary sense (fast path) | Secondary sense | What can alone trigger “help” |
|---------------------|---------------------------|-----------------|--------------------------------|
| **Life Alert / medical pendants** | **Button** (press/hold) | Optional GNSS after press | Button |
| **Silent Beacon, Birdie, Noonlight-style** | **Button** + phone app | Location, dispatch center | Button (or app SOS) |
| **Apple Watch** Fall / Crash Detection | **IMU** (accel/gyro algorithms) | GNSS, cellular, user confirm UI | IMU event → countdown → emergency call (user can cancel) |
| **Google Pixel / Samsung** car crash features | **IMU** (+ barometer sometimes) | Phone radios, user UI | High-confidence crash model → emergency flow |
| **Fitbit / Garmin safety** | **Button** and/or **IMU** incident | Phone companion | Incident detect or manual |
| **Ring / security cams** | **PIR / pixel motion** (cheap) | Full camera + cloud AI | Motion → notify; not usually auto-SMS police |
| **Bodycams (Axon-class pattern)** | **Button** + always/event record | Cloud evidence later | Manual activate; video is evidence not first “sense” |
| **Manus / StretchSense / research gloves** | **Flex + IMU** | Host PC/app | Gestures for UI/VR — rarely SOS |
| **Smart home door/window** | **Reed switch / contact** | Hub rules | Open/close events |
| **Industrial E-stop** | **Hardwired switch** | PLC logic | Switch (hard real-time, no ML) |

### Industry pattern (cheetah, in product language)

```
ALWAYS ON (cheap):   button interrupts, IMU @ 50–100+ Hz, sometimes flex/HR
ON INTEREST:         camera, full ML, livestream, cloud
HUMAN OVERRIDE:      cancel countdown (Watch) or hold-to-confirm
EVIDENCE:            video/audio *after* or *around* the event — not the only tripwire
```

**Languages on device:** almost always **C/C++** (or RTOS C) for sensor loops; app side Kotlin/Swift; ML as **pretrained** models or vendor firmware — not “train on the wrist in the alley.”

---

## 2. VisionGlove sensing policy (what *should* raise threat)

Threat levels (unchanged): **0 Safe · 1 Caution · 2 Alert · 3 Emergency**

### Policy table — which sensor may raise which level *alone*

| Sensor / input | Alone → max level | Notes (aligned with real gadgets) |
|----------------|-------------------|-----------------------------------|
| **Panic button / dedicated switch** (recommended hardware) | **3 Emergency** | Same role as Life Alert / Silent Beacon. Hard path; should not wait for camera. |
| **Triple-fist / panic gesture sequence** (flex) | **3 Emergency** | Glove-native equivalent of “deliberate SOS without a second button.” |
| **Violent IMU** (impact / thrash over threshold) | **2 Alert** | Same family as Watch fall/crash *suspicion* — prefer confirm or short fuse before police SMS. |
| **Single hard fist + high motion** | **2–3** | Escalate if combined with panic rules; alone treat as Alert unless policy says Emergency. |
| **Crowd / person_count ≥ threshold** (vision) | **1 Caution** | Like “many faces in frame” — context, not SOS by itself (Ring notifies; doesn’t auto-dial). |
| **Vision threat score mid** | **2 Alert** | Only with multi-factor score, not “blurry night = crime.” |
| **Vision threat score high + body panic** | **3 Emergency** | Eyes confirm what body already feared. |
| **Camera alone, no body/button** | **≤ 1 Caution** (policy default) | Avoid camera-only Emergency (false positives, dark, pocket, legal heat). |
| **Livestream / SMS** | N/A (actuators) | **Outputs**, not sensors — like Noonlight dispatch after trigger. |

### Duty cycle (speed vs battery)

| Layer | Rate / mode | Why (real-world parallel) |
|-------|-------------|---------------------------|
| Button | Interrupt, &lt;5 ms | E-stop / pendant |
| IMU + flex + pressure | **50–100 Hz** always-on when worn | Watch / glove controllers |
| Camera | **Off or 1–5 FPS idle**; **15–30 FPS when Alert+ or “watch mode”** | Ring: motion first; bodycam: event/mark |
| Cloud AI | **Never on critical path to first SMS** | Pendants don’t wait for a server to classify “danger” |

### Fastest detection order (product)

1. **Button** (if present)  
2. **IMU + flex rules**  
3. **Fused body + vision**  
4. **Cloud / heavy ML** (evidence, never first tripwire)

---

## 3. What we did *not* invent (standard industry)

Be honest in decks: these are **borrowed patterns**, not Strawberry Tech IP claims.

| Capability | Who already does this | VisionGlove use |
|------------|----------------------|-----------------|
| Panic / SOS button → call or SMS | Life Alert, Silent Beacon, Noonlight, watch SOS | Recommended hard Emergency |
| IMU for fall/crash / impact | Apple Watch, Pixel, Garmin | `unusual_movement`, thrash |
| Escalating alert levels | Security systems, MDM, SOC tiers | Safe → Caution → Alert → Emergency |
| Haptic patterns by severity | Watches, game controllers, cars | gentle / rapid / continuous buzz |
| Emergency SMS / dispatch | Safety apps, Twilio-backed SOS products | `EmergencyDispatcher` + SMS service |
| Person detection in frame | Phones, cameras, OpenCV/YOLO ecosystem | Person count / crowd caution |
| Dry-run / test mode for dangerous actions | Industrial & payment systems | `dry_run: true` default for SMS/stream |
| Config thresholds | Every IoT device | `config.json` |
| Pretrained CV instead of train-from-scratch | Mobile ML industry default | Architecture recommendation for v1 |

---

## 4. What VisionGlove / Grok **added or innovated** (relative to common gadgets *and* to Claude’s port)

### 4.1 Product fusion (cheetah-shaped, glove-specific)

Most consumer SOS gadgets are **button + phone**.  
Most gloves are **flex + IMU for VR**, not SOS.

**VisionGlove’s distinctive product claim:**

> A **glove-form** safety device that fuses **hand posture (flex) + motion (IMU) + grip (pressure) + optional scene (vision)** into one threat state machine that can drive **haptics + SMS + livestream** — with camera **not** allowed to be the sole Emergency tripwire under policy.

That combination as a **personal safety glove** is the product innovation space — not “we invented the accelerometer.”

### 4.2 Concrete additions in **VisionGlove_by_Grok** (this repo)

| Addition | What it is | Why it matters | Novel vs typical gadget? |
|----------|------------|----------------|---------------------------|
| **Multi-thread Stage B pipeline** | Sensor ~100 Hz, vision ~30 FPS, fusion ~50 Hz, **SPSC drop-oldest** queues | Bounded latency under load; prefers fresh fist over backlog | Stronger than Claude asyncio lists; **engineering**, same idea as robotics/drone stacks |
| **Gesture engine with hysteresis** | Stable fist/open/point/peace/thumbs-up | Avoid spam toggles | Standard technique; **implemented** (Claude stubs did not) |
| **Triple-fist panic sequence** | N fists in a time window → Emergency | Glove SOS without a second button | **Product-specific** (not on Watch); similar *idea* to coded gestures, rare on SOS pendants |
| **Multi-factor threat score** | Crowd + motion + panic + grip weighted → 0..1 then levels | Explainable fuse, not one magic ML label | Common in security fusion; **implemented end-to-end** here |
| **Hard override** | `emergency_gesture` / panic → Emergency immediately | Button-like path inside software | Standard safety design; coded in `fuse()` |
| **Vision as context, not dictator** | Policy: camera alone ≤ Caution; body/button for Emergency | Matches Ring/Watch division of labor | **Policy choice** documented for VG |
| **Staged SMS** | Alert → emergency contact; Emergency → + police/stream if configured | Graduated response | Common in safety apps; **wired** in dispatcher |
| **HMAC-SHA256 signed threat transitions** | Event integrity in logs | Tamper-evident local audit | **Uncommon** on consumer SOS toys; more “serious system” |
| **Real crypto self-test** | SHA-256 empty-vector test in `--test` | Ship checks | Engineering rigor vs Claude’s unused `token_hex` |
| **Dry-run SMS/stream by default** | No accidental real 911 SMS in demos | Safer open-source | Good practice; **enforced** in config |
| **Scenario inject for demos** | `fist` / `panic` / `shake` without hardware | Reproducible demos & tests | Dev UX; not user-facing innovation |
| **Portable C++20 core** | No Python on edge path | Sense + fuse in one language for hotpath | Matches real gadgets; **vs** Claude Python stubs |
| **Architecture stages A/B/C** | Oracle/tests vs portable core vs target kernels | Honest performance story (no fake “asm stage”) | **Process innovation** for the team deck |
| **Stimulus→SMS + sensing policy docs** | Explicit cheetah mapping + real gadget baselines | Investors/engineers share one story | Documentation productizing the design |

### 4.3 What Claude’s VisionGlove port had vs Grok

| Area | Claude repo (typical state) | Grok repo |
|------|----------------------------|-----------|
| Person / gesture / threat modules | Empty stubs | Working rules + scoring |
| Sense path | Simulated `sin()` noise | Filtered flex, IMU complementary filter, scenarios |
| Fuse | Thin ifs / incomplete | Explicit `fuse()` + hard panic override |
| SMS path | Scaffold | Dispatcher + dry-run SMS log + staged levels |
| Security | Decorative key | HMAC on transitions + tests |
| Real-time structure | Async + unbounded history | SPSC rings + fixed rates |

**Innovation relative to that baseline:** turning a **folder of stubs** into a **runnable fusion system** with tests — not inventing IMU physics.

### 4.4 What we have **not** shipped yet (don’t claim as done)

| Item | Status |
|------|--------|
| Physical panic PCB button driver | Policy yes; wire-up TBD |
| Real camera + pretrained detector (YOLO/MediaPipe) | Architecture yes; sim person count today |
| On-device training of danger AI | **Out of scope / not recommended for v1** |
| Live Twilio in open build | Dry-run first; live is integration |
| Hand-written AVX-512 / NPU kernels | Stage C optional; **0 LOC** today |
| Clinical/regulatory fall-detect certification | Not claimed |

---

## 5. Training vs live (grounded in gadgets)

| Gadget reality | VisionGlove rule |
|----------------|------------------|
| Watch fall models are **trained offline**, **run live** on IMU | Same: any ML is offline-trained, on-device live |
| Pendants **don’t train**; they use **switches** | Button + flex panic = switch-like |
| Cameras use **pretrained** person/vehicle models | VG vision v1 = pretrained or vendor API, **live frames** |
| Nobody waits for “retrain the model” during an assault | **No online training on the critical path** |

**Best + fastest for VG (same as section 2):**  
live **button + IMU + flex** first; **live camera** for context/evidence; **train offline** only to improve detectors/thresholds.

---

## 6. One-page decision for the product

| Do | Don’t |
|----|--------|
| Ship glove as **body sensors + optional camera + SOS intent** | Rely on **image-only** danger |
| Use **live** sensing always | Call a training job “sensing” |
| Use **pretrained** person detect if you need CV | Train “crime classifier” as v1 |
| Let **button / panic fist** hit Emergency fast | Wait for cloud vision before first SMS |
| Document borrowed vs added (this file) | Claim you invented the accelerometer |

---

## 7. One sentence

**Real gadgets sense with buttons and IMUs first and cameras second; VisionGlove follows that law, and what we added is a glove-native multi-modal fusion core (flex + IMU + pressure + staged vision) in real-time C++, with panic-fist Emergency, graduated SMS, signed events, and dry-run-safe defaults — implemented and tested, not just drawn as stubs.**

---

*Strawberry Tech RSA · VisionGlove · Sensing policy & innovation ledger*
