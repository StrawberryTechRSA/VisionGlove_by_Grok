# Beginner exercise 01 — Config knobs (you start here)

**No C++ required. No hardware. No rebuild** (for normal knobs).  
**Goal:** Change a setting and **see** VisionGlove behave differently.

---

## What is a config knob?

A **knob** is a value in `config/config.json` the program reads at startup.  
You turn the dial → same code, different behavior.

| Knob | File location | What it does |
|------|---------------|--------------|
| `vision.person_threshold` | `"person_threshold": 3` | How many simulated/detected people before **Caution** from crowd alone |
| `sensors.unusual_accel_threshold` | `15.0` | How hard motion must be to count as “unusual” |
| `sensors.closed_threshold` | `0.70` | How bent a finger must be to count “closed” (fist, etc.) |
| `communications.dry_run` | `true` | If true, SMS is **logged only** (safe) |

---

## Setup (once)

Open a terminal:

```bat
cd C:\Users\nuraa\VisionGlove_by_Grok
.\build.bat
.\build\visionglove.exe --test
```

You want: `Self-test: PASSED`.

Always run the exe **from the project folder** so it finds `config/config.json`:

```bat
cd C:\Users\nuraa\VisionGlove_by_Grok
```

---

## Exercise A — Crowd threshold (do this first)

### Meaning

- Config says: crowd starts at **N** people (`person_threshold`).
- CLI says: pretend the camera sees **P** people (`--persons P`).

| If… | Expect (idle hand, no panic) |
|-----|------------------------------|
| P &lt; N | Stay **Safe** (no “Threat … → Caution” from crowd) |
| P ≥ N | May go **Caution** (log: `Threat Safe -> Caution`) |

Default: **N = 3**.

### Step 1 — Baseline (below threshold)

Do **not** edit config yet.

```bat
cd C:\Users\nuraa\VisionGlove_by_Grok
.\build\visionglove.exe --demo idle --persons 2 --seconds 3
```

**Watch for:** almost no `Threat Safe -> …` from crowd (stays quiet / Safe).

### Step 2 — Baseline (at/above threshold)

```bat
.\build\visionglove.exe --demo idle --persons 4 --seconds 3
```

**Watch for:** something like:

```text
[WARN] System: Threat Safe -> Caution
```

(and maybe haptic log). **No real SMS** if `dry_run` is true.

### Step 3 — YOU turn the knob

Open in Notepad or VS Code:

`C:\Users\nuraa\VisionGlove_by_Grok\config\config.json`

Find:

```json
"person_threshold": 3,
```

Change to:

```json
"person_threshold": 10,
```

Save.

### Step 4 — Same 4 people, new rule

```bat
.\build\visionglove.exe --demo idle --persons 4 --seconds 3
```

**Expect:** with threshold 10, only 4 people should **not** raise crowd-Caution the way 4 vs 3 did.

### Step 5 — Put it back (good habit)

Set `"person_threshold": 3` again so demos/docs stay consistent. Save.

---

## Exercise B — See Emergency without hardware (optional today)

Config unchanged. This uses a **demo scenario**, not a knob — still useful:

```bat
.\build\visionglove.exe --demo panic --persons 1 --seconds 3
```

**Watch for:**

```text
Threat Safe -> Alert
Threat ... -> Emergency
[dry-run] ... SMS ...
```

That is the full path; knobs only *tune* when each step fires.

---

## Exercise C — Optional second knobs (when A feels easy)

| Try | Change | Then run | What you look for |
|-----|--------|----------|-------------------|
| Safer motion | `"unusual_accel_threshold": 25.0` | `--demo shake --seconds 3` | Harder to get Alert from shake |
| More sensitive motion | `"unusual_accel_threshold": 8.0` | `--demo shake --seconds 3` | Alert easier |
| Keep SMS safe | leave `"dry_run": true` | any demo | Always `[dry-run]` in SMS logs |

Always set sensitive values back if you use the repo for demos.

---

## Checklist — you’re done with “Start / config”

- [ ] `--test` passed once  
- [ ] Ran idle + 2 persons (quiet)  
- [ ] Ran idle + 4 persons (Caution with default threshold 3)  
- [ ] Changed `person_threshold` to 10, re-ran 4 persons (crowd less trigger-happy)  
- [ ] Restored `person_threshold` to 3  
- [ ] (Optional) Saw panic → Emergency + dry-run SMS  

**You added product behavior with your own hands — by editing JSON, not C++.**

---

## Next (only after this checklist)

**Exercise 02 — New gesture** (C++): one new hand meaning + one self-test.

---

## If something fails

| Problem | Fix |
|---------|-----|
| `config` not found | `cd` to `VisionGlove_by_Grok` before running |
| No exe | Run `.\build.bat` |
| JSON error / weird defaults | Check commas; use valid JSON; compare to git version |
| No Caution ever | Confirm you saved JSON; use `--persons` high enough vs threshold |

---

*Strawberry Tech RSA · VisionGlove · Beginner 01*
