# Cheetah mapping — design method + X-ready posts

**Product:** VisionGlove (Strawberry Tech RSA)  
**Code:** [VisionGlove_by_Grok](https://github.com/StrawberryTechRSA/VisionGlove_by_Grok)  
**Related:** [SENSING_POLICY.md](./SENSING_POLICY.md) · [STIMULUS_TO_SMS.md](./STIMULUS_TO_SMS.md) · [ARCHITECTURE_ONE_PAGER.md](./ARCHITECTURE_ONE_PAGER.md)

---

## What “cheetah mapping” is

We did **not** build a cheetah simulator.

We used the **cheetah as a sensing design metaphor**: how a predator stays alive — **many senses, ranked by speed**, then **one decision** (freeze / run / call others).

Then we **mapped each biological sense → a VisionGlove channel → a product rule**, aligned with how real gadgets already work (Watch IMU first, pendant button first, camera second).

That process is **cheetah mapping**.

**Quote line:**

> Body first. Eyes second. Commitment is Emergency. SMS is the roar — not the ear.

---

## The map (show this)

| Cheetah | Survival role | VisionGlove | Product rule |
|---------|---------------|-------------|--------------|
| Muscle / balance / bad landing | Instant body alarm | **IMU** (accel/gyro) | Fast path → Alert on thrash/impact |
| Limb position | Know posture without looking | **Flex sensors** | Fist / patterns; **triple-fist = panic SOS** |
| Paw pressure / contact | Grip, struggle | **Pressure sensors** | Tight grip supports higher threat |
| Eyes | Confirm *what* is out there | **Camera / person detect** | Context: crowd → Caution; **not** sole Emergency |
| Commit (bolt / fight) | Full response | **Panic button** (recommended) or **panic fist** | Hard **Emergency** → SMS / stream |
| Brain (fuse signals) | Combine, act | **`fuse()` levels 0–3** | Only **level change** fires haptics + SMS |
| Call the pride | Signal others | **SMS + livestream** | **Outputs**, not sensors |
| Skills before the hunt | Learn offline | **Stage A tests / goldens** | Train offline; **live** sense in the field |

```
[CHEETAH]        [VISIONGLOVE]              [OUTPUT]
 balance      →  IMU                        haptics
 posture      →  flex (panic fist)          SMS
 eyes         →  camera (context)           livestream
 commit       →  fuse → Emergency           contacts = "pride"
```

---

## How we did the mapping (method)

1. **Start from biology, not buzzwords**  
   What does a cheetah use *before* a perfect picture of danger?  
   → Motion, balance, body — not a slow, perfect photo.

2. **Translate each sense into real hardware classes**  
   - Balance → **IMU** (same family as Apple Watch fall/crash)  
   - Posture → **flex** (same family as VR/research gloves)  
   - Eyes → **camera** (phones / Ring — secondary)  
   - Commit → **button / panic gesture** (Life Alert / SOS pendants)

3. **Rank by speed** (policy innovation, not new physics chips)  
   1. Button / panic fist (fastest intent)  
   2. IMU + flex @ ~50–100 Hz (always-on body)  
   3. Vision @ ~30 FPS, duty-cycled (context)  
   4. Cloud / heavy ML — **never** on first SOS path  

4. **Write fuse rules so one sense can’t lie alone**  
   - Camera alone → **≤ Caution**  
   - Violent motion → **Alert**  
   - Panic fist / button → **Emergency**  
   - Body + bad scene → escalate together  

5. **Implement in C++ Stage B**  
   Sense threads → snapshots → `fuse()` → haptics → SMS dispatcher.  
   Mapping = story; repo = proof.

6. **Honest ledger**  
   Mapping ≠ inventing the accelerometer.  
   Mapping = **glove-native multi-sense SOS policy** vs Watch / pendant / Ring baselines  
   (see [SENSING_POLICY.md](./SENSING_POLICY.md)).

---

## Why this is defensible on X / in decks

- **Visual** — animal → glove → SMS  
- **Memorable** — not only “multi-modal sensor fusion”  
- **Aligned with real gadgets** — body first, camera second  
- **Tied to code** — C++ fusion, staged SMS, dry-run defaults  

---

## X post — single (copy-paste)

```
We designed VisionGlove with “cheetah mapping.”

A cheetah doesn’t wait for a perfect photo of danger.
Body first (balance, posture) → eyes second → then commit.

Map:
• Muscle/impact → IMU
• Limb posture → flex (triple-fist = SOS)
• Eyes → camera (context, not sole emergency)
• Commit → panic path → SMS

Same law as real gadgets: Watch/pendants go IMU+button first; cameras second.

We implemented it in C++: sense → fuse (0–3) → haptic → SMS.
Not vibes. Policy + code.

https://github.com/StrawberryTechRSA/VisionGlove_by_Grok
docs: CHEETAH_MAPPING.md · SENSING_POLICY.md · STIMULUS_TO_SMS.md
```

---

## X thread (copy-paste, 6 posts)

**1/**  
VisionGlove sensing = **cheetah mapping**.  
Not a mascot — a design method.

**2/**  
Cheetah survival stack:  
body alarm → posture → eyes → commit → signal the pride.

**3/**  
Map to glove:  
IMU · flex · pressure · camera · panic fist/button · fuse · SMS.

**4/**  
Rule we locked:  
**Eyes alone can’t declare Emergency.**  
Body/button can. Camera confirms the world.

**5/**  
Why? Real gadgets already work that way (Watch IMU, Life Alert button, Ring motion-then-video).  
We applied it to a **safety glove**.

**6/**  
Built in C++: multi-thread sense → fuse levels 0–3 → haptics → dry-run-safe SMS.  
https://github.com/StrawberryTechRSA/VisionGlove_by_Grok  
https://github.com/StrawberryTechRSA/VisionGlove_by_Grok/blob/main/docs/CHEETAH_MAPPING.md

---

## Image / slide caption

**Title:** Cheetah mapping → VisionGlove  
**Subtitle:** Fast senses decide. Eyes refine. SMS is the call for help.

**One-sentence slide line:**

> Cheetah mapping ranks VisionGlove sensors the way a predator ranks survival signals—body and intent first, vision second—then fuses them so SMS fires on a deliberate threat level change, not on a lonely camera frame.

---

## What we innovated vs borrowed (short)

| Borrowed (industry) | Added (VisionGlove / this repo) |
|---------------------|----------------------------------|
| IMU first (Watch) | Glove-native **flex panic sequence** |
| Button SOS (pendants) | **Multi-modal fuse** flex+IMU+pressure+vision |
| Camera secondary (Ring) | Policy: **camera alone ≤ Caution** |
| SMS dispatch (safety apps) | **Staged** Alert contact vs Emergency + stream |
| C/C++ sensor loops | **Working C++ core** + SPSC + tests (not stubs) |

Full ledger: [SENSING_POLICY.md](./SENSING_POLICY.md).

---

*Strawberry Tech RSA · VisionGlove · Cheetah mapping*
