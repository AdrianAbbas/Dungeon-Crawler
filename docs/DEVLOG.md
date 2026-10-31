# DUNGEON CRAWLER development log

Decisions, problems and fixes, newest milestone last. The brief is in
[PROJECT_BRIEF.md](PROJECT_BRIEF.md). The build, install and validation pipeline comes
from WORLDBUILDER (https://github.com/AdrianAbbas/Worldbuilder), which took it from VEIL
(https://github.com/AdrianAbbas/Dark-Fantasy-Plugin); the sound modules come from VEIL.

## Progress checklist

A new milestone starts its own block here; a resumed session continues from the first
unticked item.

**Milestone 0: setup + audit** (2026-10-31): done, waiting for the GarageBand check.

- [x] 1. Pipeline from Worldbuilder, adapted for an effect: CMake, CI workflow, scripts, packaging, AU host harness
- [x] 2. VEIL DSP audited and imported (six modules, gain stages), defects fixed
- [x] 3. Minimal effect skeleton: engine with travel between ten sketched locations, host parameters, state, editor
- [x] 4. Linux test suite (244 checks) passing, also under AddressSanitizer + UBSan; CPU report
- [x] 5. DEVLOG, README, manual test; pushed without `[skip ci]`
- [x] 5b. First macOS CI run: one harness gate failed (Intensity 0 at -39.6 dB); fixed, pushed again
- [x] 6. **Developer:** GarageBand check on the MacBook Air: loads, passes audio, the location list switches (confirmed)

**Session 2** (2026-10-31): Milestone 0 fixes, Milestone 1, Milestone 2a.

- [x] 0a. Intensity 0 is a true, latency-aligned dry path (30 ms crossfade; residual −272 dB)
- [x] 0b. AUAudioUnit out-of-process route rendered through AVAudioEngine, now a hard gate (AULowpass control too); macOS job on `macos-15`
- [ ] 1a. Place data model: 10 locations, all sub-locations, Sub-location parameter (automatable)
- [ ] 1b. Travel Time (instant, seconds, tempo-synced); crossfade of incompatible states; tails continue
- [ ] 1c. Level matching across places (calibrated per-place trims + matcher)
- [ ] 1d. Macros (3–4 themed per location) and the Advanced drawer; simple editor (location list, sub-location list, macros)
- [ ] 1e. Tests, DEVLOG, push `[skip ci]`
- [ ] 2a-1. Sound design: Dungeon, Castle, Cathedral, Haunted Forest, Caverns with all sub-locations
- [ ] 2a-2. Distinctness test across all places + closest pairs listed
- [ ] 2a-3. Listening render (same guitar and synth phrase through every place) to WAV in the test-logs artifact
- [ ] 2a-4. DEVLOG, manual test, final push without `[skip ci]`

---

## Lessons carried over (from Worldbuilder's DEVLOG, read first)

- **GarageBand silence was a Gatekeeper rejection, not a DSP bug.** A browser download
  marks every file with `com.apple.quarantine`; macOS then refuses to load an ad-hoc
  signed bundle into `AUHostingServiceXPC_arrow`, and GarageBand shows a silent slot
  with generic sliders. `install-mac.sh` removes the flag, re-signs, resets
  `AudioComponentRegistrar` and runs `auval`. CI installs a quarantined copy with the
  script before validating it.
- **CI `auval` passing does not prove GarageBand works.** GarageBand on Apple Silicon
  hosts AUv2 plug-ins out-of-process; the AU host harness exercises the in-process and
  out-of-process routes (both gates) on arm64 and x86_64.
- **Sign whole bundles** (`codesign --force --deep --sign -`) before packaging; publish
  the installable artifact only when every check passed.
- **Universal binary, 10.15, `AU_SANDBOX_SAFE TRUE`, JUCE 8.0.16.**
- **Out-of-process rendering needs host-like pacing** (one block per block duration,
  host time set, a pause before teardown). The harness keeps VEIL's pacer.
- **JUCE publishes continuous AU parameters normalised to 0–1.** The harness sets them
  through min/max or `ParameterValueFromString`.
- **The AUAudioUnit route as driven by the harness is unreliable** (silence even for
  Apple's AULowpass). It stays diagnostic, with AULowpass as the control.
- **Worldbuilder's GarageBand freezes were the message thread flooded with frames**
  and expensive per-frame drawing on a fanless Air. Hence the brief's editor rules.
  The Milestone 0 editor has no timer at all.
- **Annotations are cut at about 4 KB**, at most 10 per step: logs are condensed and
  chunked (`ci-annotate.sh`).

---

## Milestone 0: setup + audit (2026-10-31)

### Working conditions

| | |
|---|---|
| Machine | Linux cloud container (Ubuntu 24.04, x86_64). No macOS, Xcode, GarageBand or Logic. |
| Verified here | Linux build (VST3, Standalone, tests) with JUCE 8.0.16: **244 checks pass, 0 fail**, also under AddressSanitizer + UBSan (regular groups). Editor rendered to an image and inspected. |
| Not verified here | Anything macOS: the AU build, `auval`, the adapted harness (it has **never been compiled**: CI is its first compile), the install script with the new identity, GarageBand. |

### Identity

| | |
|---|---|
| Type | `aufx` (`kAudioUnitType_Effect`), audio in → audio out, no MIDI |
| Subtype / manufacturer | `Dgcr` / `Veil` |
| Name in hosts | "Veil Audio: DUNGEON CRAWLER" (`PLUGIN_NAME`) |
| Files and bundles | `DungeonCrawler.component`, `.vst3`, `.app` (`PRODUCT_NAME`, no space: simpler, safer scripts) |
| Bundle ID | `audio.veil.dungeoncrawler` |
| Factory function | `DungeonCrawlerAUFactory` (`AU_EXPORT_PREFIX DungeonCrawlerAU`) |
| Formats | AUv2, VST3, Standalone (dev only, not shipped in the zip) |
| Layouts | stereo→stereo, mono→mono, mono→stereo |

### 1. Pipeline (from Worldbuilder)

Copied and renamed: `.github/workflows/build.yml`, `scripts/` (`ci-fetch-juce`,
`ci-check-bundles`, `package-mac`, `build-au-harness`, `ci-user-install-test`,
`ci-annotate`, `ci-crash-summary`, `ci-developer-id`, `install-mac`, `uninstall-mac`,
`diagnose-mac`, `register-au`, `run-tests-parallel`), `packaging/INSTALL.txt`.
Adaptations:

- **Removed:** the private-samples fetch (`ci-fetch-samples.sh`, `SAMPLES_TOKEN`), the
  samples gate in `ci-check-bundles.sh` and the installer's sample report.
- **Workflow:** macOS job builds universal, runs `--only smoke,cpu` on each slice
  (arm64, x86_64 under Rosetta; the cpu group only reports), checks the bundles
  (`aufx`/`Dgcr`/`Veil`, `DungeonCrawlerAUFactory`, `sandboxSafe`), signs and packages,
  installs a quarantined copy with `install-mac.sh`, then `auval` and the harness on
  both architectures. Linux job: full suite in three parallel shards plus the cpu
  group alone. ccache on both. Tagged commits publish a release. No step name contains
  `: ` unquoted.
- **Installer and INSTALL.txt:** effect-slot instructions (Audio Effect slot in
  GarageBand, Audio FX in Logic), logs in `~/Library/Logs/DungeonCrawler`.
- **AU host harness (`tools/au-harness/DungeonCrawlerAUHarness.mm`).** Worldbuilder's
  harness is VEIL's effect harness with the input bus taken out and instrument checks
  added; for an effect the right base is the effect form. So it is VEIL's harness (the
  same pacer, routes, editor and state checks as Worldbuilder's) with the identity
  changed and these checks per route:
  - the plug-in pulls its input; both outputs written; output finite and audible;
  - host bypass and Mix 0 + Output 0 dB pass the input through (residual < −40 dB);
  - Output to minimum is audible;
  - **Intensity 0 passes the input through** (< −40 dB);
  - **Location is accepted and changes the sound**: Village vs Cathedral residuals must
    differ by > 3 dB. The Linux smoke test replays the harness's exact sequence:
    0.9 dB vs −6.0 dB (6.9 dB apart);
  - factory presets: reported only (journeys come in Milestone 5);
  - state round trip into a fresh instance; 44.1 / 96 kHz; mono→mono, mono→stereo;
    the editor (Cocoa view in-process, remote view controller out-of-process).
  The AUAudioUnit route stays diagnostic, with Apple's AULowpass (`aufx:lpas:appl`) as
  its control (Worldbuilder used the DLS synth, an instrument).

### 2. Audit of VEIL's DSP

VEIL `main` (59b6696): `Source/DSP/{Common.h, GainStage.h, Modules.h, Abyss, AncientBlood,
Choir, Wraith, Echoes, Crypt}`, about 1,400 lines. Read in full; everything real-time
safe (allocation in `prepare`, no locks, NaN guards, bounded feedback).

| Module | Verdict | Notes |
|---|---|---|
| **Common.h** (TPT SVF, RBJ biquads, delay line, granular shifter, DC blocker, smoothers) | keep as is | Correct and stable; namespace `veil` → `dc`. |
| **Abyss** (12/24 dB resonant low-pass, high-pass, Darkness, Movement, Tomb Resonator) | keep | Clean; coefficient updates every 16 samples. Its On switch moves into the engine. |
| **Ancient Blood** (2× oversampled saturation, loudness compensation, Age, Dust, Instability) | **fixed** | Drive, Texture, Age, Dust and Instability were applied as raw *targets*: any change jumped drive gain, wow depth and bit depth at once. Fine for a knob, a click for a morph. Now they follow their 30 ms smoothers, sampled per 64-sample chunk (test: a 0 → 36 dB drive jump steps 0.85× the steady signal). The stage runs while Blood Mix > 0 (no switch). |
| **Choir** (1–4 granular voices, vowel formants, Drone) | keep | Interval changes switch the ratio without a glide (no forced pitch glides, as the brief asks). |
| **Wraith** (STFT phase vocoder: blur, detune/drift, ghost, freeze) | keep | ~43 ms frames at any rate; wet-only layer, its STFT delay is part of the effect. |
| **Echoes** (crossfaded time changes, damping, diffusion, Reverse, Descent, spread) | **fixed** | Reverse: one shared segment length glided quickly (42 ms) once a time crossfade finished, warping both reverse heads mid-segment: a click after every time change (found as a click 80 ms after arriving in the Necropolis). Each head now latches the current time when it wraps, where its window is zero. |
| **Crypt** (8-line Householder FDN, diffusion, early reflections, shimmer, Rising, Eternal) | **fixed** | Size and chamber changes were unusable for morphing: (1) all eight line lengths glided with a 52 ms time constant: a Doppler warble of up to ±80 % pitch in the tail; (2) the early-reflection pattern, (3) the reflection scale (Space), (4) the diffuser count, (5) the modulation depth and (6) the body EQ switched at once; (7) loop gains and injection stepped per chunk, and the step re-emerged one line length later. Now: line-length changes crossfade two taps of the same line over 80 ms (no pitch change, tails continue); reflection patterns crossfade; the reflection scale, modulation depth and body EQ are smoothed; diffuser stages fade in and out per sample (all eight keep running); loop gains and injection glide per sample. Test: chamber + size changes on a held sine step 0.91× the steady signal (VEIL code: 1185×). |
| **GainStage.h: LevelMatcher** | **extended** | Keeps the wet level at the input level while the input sustains, holds during decays. During travel the chain's raw level changes faster than its 1 s windows follow, and the held gain overshot into the limiter (on a sine: +7 dB, flattened peaks). An *agile* mode (0.15 s windows, 50 ms glide) runs while travelling and 0.5 s after. |
| **GainStage.h: SafetyLimiter** | **fixed** | The release could rise past the gain a peak needed and let it through (0.974 on +36 dB noise for a 0.966 ceiling). Release is now capped at the needed gain. |
| Dread macro, Ritual Engine, presets, interface | discarded | As the brief says; Intensity and (Milestone 1) the macros replace Dread. |

Not changed, noted for later: the Choir and Wraith run their full voice/FFT work
whenever active (2–5 % of a core per place here, see CPU below), and Blood's
loudness compensation tracks only 80 ms windows.

### 3. The effect skeleton

- **Module parameters** (`Source/DSP/ModuleParams.h`): VEIL's module parameters only
  (52), each with a *neutral* (transparent) value and a glide type: Lin, Log (cutoffs,
  times: geometric) or Step (choices switch half-way). The per-module On switches are
  gone: the engine runs a module while the frame asks anything of it (a mix above zero,
  the Abyss away from open) and skips it otherwise.
- **Locations** (`Source/World/Locations.h`): the ten places of the brief as **first
  sketches** (Dungeon, Castle, Cathedral, Haunted Forest, Caverns, Village, Swamp,
  Mountains, Necropolis, Wizard's Tower), each a short list of changes from neutral.
  The real sound design is Milestone 2.
- **Engine** (`Source/Engine.*`): Abyss → Ancient Blood → Choir → Wraith → Echoes →
  Crypt → level matcher, in 64-sample chunks. A location change *travels* for 1.5 s
  (smoothstep): from wherever the sound is (even mid-journey) to the new place's frame;
  Log params glide geometrically, Step params switch half-way. Intensity scales a
  place between neutral (0, transparent) and as designed (1). A module that starts
  again starts clean, with its input faded in over 5 ms (otherwise the hard onset came
  back as a click on the first echo).
- **Host parameters** (`Source/Parameters.h`): Location (choice, 10, automatable),
  Intensity, Mix, Input, Output, Bypass (the host bypass parameter). Stable versioned
  IDs; Location's order is fixed.
- **Processor:** VEIL's frame (input/output channel handling, dry path delayed by the
  reported latency, 20 ms gain smoothing, 25 ms bypass crossfade, −0.3 dBFS limiter).
  Latency: 124 samples at 48 kHz (Blood's oversampler and wow centre), constant.
  `reset()` keeps tails (hosts call it on stop); one program whose
  `setCurrentProgram` does nothing; state is validated (size, tag, NaN rejects the
  whole state, values clamped, missing parameters keep defaults).
- **Editor** (960 × 540): title with a red initial, the ten places as buttons with the
  active one marked, four tall slim faders (ivory caps, red value track), Bypass. The
  hatched, framed background is drawn once per size into an image; there is **no
  timer**, so nothing repaints unless a control changes (1.1 ms per full software
  repaint). Fonts are system fonts for now. Snapshot: [snapshots/m0-editor.png](snapshots/m0-editor.png).

### Decisions

- **No Travel Time control yet**: fixed 1.5 s; Milestone 1 adds the control (instant →
  seconds or tempo-synced).
- **Choices never interpolate** and Intensity keeps the place's choice values (with the
  mixes at zero they are inaudible), so Intensity never flips a chamber half-way.
- **Input drives the place, it does not change the level**: the level matcher's
  reference is the raw input, so Input sets how hard Blood and the resonances are hit,
  while loudness stays matched. Output sets the level.
- **Level matching for now is adaptive only.** Measured level of every place against
  the input: −0.15 to +0.34 dB on a sustained chord, −0.53 to +0.53 dB on plucks;
  loudness while travelling stays within 3.6 dB of the level before leaving (worst:
  to the Cathedral). Milestone 1 adds calibrated per-place trims so the matcher only
  trims.
- **Mood image not used or committed**: the look is drawn in code from the brief's
  description (charcoal, hatching, ivory, one red accent).

### Tests (Linux, through `processBlock`), 244 checks

`Tests/DungeonCrawlerTests.cpp`, groups: `parameters`, `smoke`, `state`, `layouts`,
`transparency`, `locations`, `travel`, `modules`, `rates`, `extremes`, `realtime`,
`editor`, and `cpu` (report only, run explicitly). `--snapshot f.png` writes the
editor; `--render f.wav <location> [seconds]` renders plucks and a chord through a
place (listening aid).

- Mix 0, Bypass: the input exactly (latency-aligned, < −100 dB); Intensity 0 < −60 dB.
- Every place level-matched within 3 dB, below 0 dBFS, and **every pair different**
  (octave bands and tail: the closest pair differs by > 2 dB somewhere).
- **Travel from the Dungeon to every place**: no click (largest sample step during the
  journey at most 1.38× the steady signal's), loudness within 4 dB, arrives.
- Forty random destination and Intensity changes mid-journey at odd block sizes:
  finite, below 0 dBFS.
- 44.1–192 kHz, blocks 1–4096 and varying; NaN/Inf/denormal input; full-scale noise
  at +36 dB stays below −0.3 dBFS; every place silent 15 s after the input stops.
- No heap allocation in `processBlock` while travelling, changing Intensity and
  bypassing.
- Editor: size, marker follows clicks and automation, a full repaint < 33 ms.

CPU (one core of this container, 48 kHz stereo, report only): 1.8 % (Castle) to 5.5 %
(Necropolis); travelling every second 5.9 %.

### First macOS CI run

Reported by the developer from the run of e5d8791:

- Install from a quarantined (Chrome-style) download, `auval` arm64 and x86_64: **pass**.
  (`auval` on the quarantined copy without the script also passed: as for VEIL, the
  runner does not reproduce the Gatekeeper rejection.)
- Harness, AUv2 in- and out-of-process, arm64 and x86_64: **every check passed but one**:
  "Intensity 0 passes the input through" measured −39.6 dB against −40 dB.
  **Cause:** going from the Dungeon to Intensity 0, the level matcher kept the
  Dungeon's correction and released it only through its 1 s windows; 2 s later about
  1 % remained. The Linux test had started fresh at Intensity 0, so it never saw that.
  **Fix:** when nothing in the chain is active the matcher is disabled (it returns to
  unity), and an Intensity change makes it agile for 0.5 s, as travelling does. The
  smoke group now replays the harness's sequence: −82.9 dB.
- AUAudioUnit route (diagnostic, not a gate): silence, as for VEIL and Worldbuilder with
  this harness route. AULowpass control exit 0.
- No crash reports.

### Milestone 0 handoff

- **What to test in GarageBand:** [MANUAL_TEST.md](MANUAL_TEST.md), section M0.
- **If the CI fails:** read the last ~100 lines of the failing step; the harness is
  compiled for the first time there.
- **Open for Milestone 1:** sub-locations (data model), Travel Time, per-place trims,
  macros, Advanced drawer, crossfading incompatible states (for example a Choir
  interval change during travel switches half-way today).
