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
- [x] 1a. Place data model: 10 locations, all sub-locations, Sub-location parameter (automatable)
- [x] 1b. Travel Time (instant, seconds, tempo-synced); crossfade of incompatible states; tails continue
- [x] 1c. Level matching across places (calibrated per-place trims + matcher)
- [x] 1d. Macros (3–4 themed per location) and the Advanced drawer; simple editor (location list, sub-location list, macros)
- [x] 1e. Tests (512 checks), DEVLOG, push `[skip ci]`
- [x] 2a-1. Sound design: Dungeon, Castle, Cathedral, Haunted Forest, Caverns with all sub-locations
- [x] 2a-2. Distinctness test across all places + closest pairs listed; musicality group (guitar, synth, piano, vocal)
- [x] 2a-3. Listening render (same guitar and synth phrase through every place) to WAV in the test-logs artifact
- [x] 2a-4. DEVLOG, manual test, ASan run, final push without `[skip ci]`

**Next: Milestone 2b** (sound design of Village, Swamp, Mountains, Necropolis, Wizard's
Tower: all their places to the 10 dB-eq distinctness gate; the Village's three places
are currently the closest in the world, 6.2–7.7). Then Milestone 3 (art pipeline + map).

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

---

## Session 2: Milestone 0 fixes (2026-10-31)

The developer confirmed Milestone 0 in GarageBand: it loads, passes audio, the location
list switches.

### Intensity 0: a true dry path

Milestone 0 made Intensity 0 transparent by returning the level matcher to unity at a
neutral frame (CI: −82.9 dB). It was still the chain at neutral settings, not the
input. Now the processor crossfades (30 ms, linear) to the **latency-aligned input**
(times Output) whenever Intensity is 0: the residual is −272 dB (bit-exact apart from
float rounding), from a fresh start, after a place, and within 0.1 s of moving the
fader to 0 mid-chord, for every location (tests `smoke`, `transparency`).

### The AUAudioUnit route: rendered by AVAudioEngine, now a gate

The harness drove the out-of-process AUAudioUnit itself: it fetched `renderBlock` and
called it with its own pull-input block. On that route even Apple's AULowpass came
back silent or with −10876 (`kAudioUnitErr_NoConnection`). That points at the harness's
way of connecting the input bus, not the plug-in. The suite now does what hosts do:

- `AVAudioUnit instantiateWithComponentDescription:` with `LoadOutOfProcess`;
- an `AVAudioEngine` in real-time **manual rendering** mode (48 kHz stereo, 512 frames)
  with an `AVAudioSourceNode` (the sine) → the unit → the output node;
- rendering paced like a host (one block per block duration) through
  `manualRenderingBlock`.

Its checks: render status, input pulled, audible, right channel written, Output to
minimum via the `AUParameterTree`, **Intensity 0 transparent (< −40 dB)**,
`shouldBypassEffect` transparent, factory presets (reported), `fullState`, and the
remote view controller. In `ci-user-install-test.sh` the route is now a **gate** on
arm64 and x86_64, and so is the AULowpass control on the same route. The control
tells a harness problem from a plug-in problem.

**Not yet verified on macOS.** This session has no Mac; the next CI run is the
route's first. If it fails, read the `harness-*-auaudiounit` and
`control-apple-lowpass-auaudiounit` logs: if the control also fails, the harness is
still at fault.

The macOS job runs on `macos-15`.

---

## Milestone 1: the place engine (2026-10-31)

### The world (data model)

`Source/World/Locations.{h,cpp}`: ten locations, each with

- its own **scene** (sub-location 0, the place you arrive at): Corridors, Courtyard,
  Cloister, Forest Edge, Cave Mouth, Village Green, Mire, Foothills, Gates, Stairwell;
- the brief's **sub-locations** (2–4 each, 30 in all, so **40 places**);
- **3–4 themed macros** with their mappings.

A place's frame is the location's base changes with the sub-location's changes on
top. The data is built once at static initialisation and only read after that.

### Host parameters (`Source/Parameters.h`)

| Parameter | Notes |
|---|---|
| Location | choice, 10, automatable (unchanged) |
| **Sub-location** | int 0–4, automatable; its text is the current location's sub-location name ("Treasure Room"). Above a location's count it means the last one. Choosing a location in the editor arrives at its scene (0). |
| **Travel Time** | 0–10 s (skewed, default 1.5 s); below 20 ms reads "Instant" |
| **Travel Sync** | Free, 1/4, 1/2, 1 bar, 2 bars, 4 bars (host tempo, 120 BPM without one) |
| **Macro 1–4** | 0–1, 0.5 = as designed, shown as −100…+100 %. Names follow the location (Dungeon: Depth, Damp, Echo, Dread). |
| **Adv …** (61) | the Advanced drawer: one per module parameter. Continuous: an **offset** (−100…+100 % of the range; Log ranges in ratios of the whole range). Choices: an **override** ("Place" = as designed). Offsets travel with you. |

Order of application: place → macros → Advanced → Intensity (neutral … as designed).
State version 2; older states load with the new parameters at their defaults.

### Travel and morphing (`Source/Engine.*`)

- **Controls** are read once per host block (`setControls`). A change of place starts
  a journey from wherever the sound is, over Travel Time. "Instant" is 20 ms, still
  click-free. Macros, Advanced and Intensity change the target in place: the modules
  smooth continuous values, and a choice switches through a 50 ms journey.
- **Incompatible states crossfade.** Some choices cannot morph: the Choir's voice
  count and interval, the Tomb's note and octave, the Relics' metal kind, note and
  octave. When a journey changes one, that layer dips out. It stays silent for about
  80 ms on each side of the switch, fading over about 120 ms, and the switch happens
  in the silence. Such journeys last at least 0.4 s. Other choices already crossfade
  inside their modules (Crypt chamber, Echoes reverse and descent, Abyss slope).
- **Reverb tails continue.** The Crypt runs as a pool of three rooms. A journey to a
  place with a different room starts the new room in the quietest free instance and
  hands the send over (equal power, along the journey's curve). The old room gets no
  new input and **rings out with its own decay**. Within a place, the active room
  follows the frame. The Crypt and the Echoes also hold their return level: when
  their mix falls, the return falls no faster than the tail decays, while new input
  enters at the new mix. Test: leaving the Nave for the Tavern as the music stops,
  the tail 1–1.5 s later is at −46 dBFS (the Tavern alone: −105 dBFS), and it decays
  smoothly.

### Level matching across places

- **Calibrated trims** (`Source/World/LevelTrims.h`, generated by
  `DungeonCrawlerTests --calibrate Source/World/LevelTrims.h`). Each place's raw level
  is measured with the matcher off, as the mean of a sustained saw chord and guitar-like
  plucks; the trim is its negative, scaled by Intensity, and interpolated in dB while
  travelling. The adaptive matcher then only fine-tunes.
- The `levels` group re-measures every place and **fails if a stored trim is more than
  1 dB off**, so trims cannot silently go stale after sound-design changes.
- Result for all 40 places: chord within ±0.6 dB of the input, plucks within ±1.3 dB.
  Loudness while travelling stays within 2.4 dB of the level before leaving.

### New module: VII. Relics (`Source/DSP/Relics.cpp`)

The brief asks for additions beyond VEIL's modules: torch-flicker tremolo, metallic
resonators, bell resonances, and gentle noise beds where fitting. Relics sits after
Ancient Blood, so the echoes and reverb carry what it adds:

- **Torch Flicker** (+ rate): an irregular tremolo (a random walk plus a slow sine),
  up to 55 % dips.
- **Metal** (+ kind, note, octave, decay): six SVF band-pass resonators, unity peak
  gain, with a T60 per partial. Iron bars (plate-like modes), a minor-third church
  bell (hum, prime, tierce, quint, nominal), and crystal. It is **keyed**: note and
  octave are choices, so a producer can tune it to the song in the Advanced drawer.
- **Wind** and **Drips**: band-passed noise with a slowly moving centre, and sparse
  rising-pitch water drops. Both follow the input's envelope (20 ms attack, 1.2 s
  release), so they fall silent with the playing (never a constant noise floor).

### The editor (simple, Milestone 1)

960 × 560: the locations as a list, the current location's places (its scene plus
sub-locations) as a second list, the title "Location › Place", the location's macros
as faders under their themed names (unused macros hidden), Input, Intensity, Mix and
Output, Travel (time and sync), Bypass, and **Advanced**. That drawer covers the right
side: the 61 module parameters in two columns grouped by module, sliders for offsets
(the red track starts at 0) and menus for overrides. Still no timer. Repaint: 1.8 ms;
15 ms with the drawer open. Snapshots: [snapshots/m1-editor.png](snapshots/m1-editor.png),
[snapshots/m1-editor-advanced.png](snapshots/m1-editor-advanced.png).

### Tests (512 checks, Linux)

- New group `place`: sub-location journeys; Travel Time Instant (≤ 40 ms), 3 s, and
  1 bar = 2 s at 120 BPM; the Choir dip at the switch; Treasure Room → Boss Lair
  without clicks, instant and 1.5 s; an Advanced Interval override without a click;
  tails continue; every Dungeon macro audible both ways; Advanced Cutoff and choice
  overrides.
- New group `levels`: stored trims against measurements.
- `parameters`: the brief's sub-locations for every location, 3–4 macros each, every
  macro changes the place, 50 % leaves it as designed, every Advanced parameter
  published, Sub-location's text.
- `locations` now covers all 40 places: level (chord ±3 dB, plucks ±4 dB), peak, and a
  **distinctness fingerprint** (report-only here, a gate in Milestone 2a; see below).
- `realtime` also changes sub-locations, macros and Advanced while checking for heap
  allocations.

CPU (one core of this container, 48 kHz stereo, report only): 3.0 % (Wizard's Tower)
to 7.0 % (Necropolis); travelling every second 9.5–10 % (two rooms and the dips).

---

## Milestone 2a: sound design, first five locations (2026-10-31)

### The places

Every place has a **signature**: a combination of room (size, chamber, decay,
pre-delay), brightness, echo pattern, movement, pitch layers, resonances and beds
that no other place shares. Levels are matched by the calibrated trims.

| Place | Signature |
|---|---|
| **Dungeon · Corridors** | dark, damp mid-sized stone room (Underground Crypt chamber, 2.5 s), short diffuse slap down the passage, torch flicker, distant drips, a touch of grit |
| Entrance Hall | a bigger, darker vault (3.5 s, 38 ms pre-delay), the strongest torch flicker, a wide slap from the far door |
| Prison Cells | tiny boxy cell (0.7 s), muffled resonant low-pass at 1.9 kHz, a 55 ms flutter between the walls, iron bars ringing (Relics metal, D), grime (Age, Dust), drips |
| Treasure Room | the brightest: crystal glints (A), an octave of sparkle above, shimmer, ping-pong echoes on the eighth note |
| Final Boss Lair | huge and menacing: 18 dB drive, a three-voice choir a fifth below with a drone, a low D resonance, slow echoes that sink a semitone each time, the Endless Void (8 s), slow flicker |
| **Castle · Courtyard** | open air between walls: a crisp, wide 210 ms slap, a short open reverb, a breeze, thinner lows |
| Great Hall | timber over stone: warm (damped, a little drive), wide, medium-large (2.8 s), a fire's flicker |
| Throne Room | regal: the widest bright hall (4.2 s, 50 ms pre-delay), an organ-like fifth above, a halo of shimmer |
| Chapel | sacred: a long reverb (6.5 s) with a soft choir an octave below and a drone |
| Tower Top | thin (230 Hz high-pass), windy, distant: echoes across the valley (520 ms), a smeared haze, a small far reverb |
| **Cathedral · Cloister** | arcades: a 92 ms flutter between the arches feeding a long, bright stone reverb |
| Nave | the longest, wettest place in the world (12 s, 80 ms pre-delay), a breath of choir |
| Crypt | very dark and close (1.5 kHz, 2 s, heavily damped), a low D resonance, a murmur an octave down, drips |
| Bell Tower | the instrument strikes a minor-third church bell (Relics bell, D3, 5 s), wind through the louvres, open echoes |
| **Haunted Forest · Forest Edge** | soft damped open air, strong wind in the leaves, a faint spectral haze, diffuse echoes from the trunks |
| Old Path | eerie: strong spectral ghosts (Wraith blur, ghost, drift), echoes played backwards, slow filter movement, dark |
| Witch's Hut | a small wooden hut (0.5 s), woody saturation and wow, a candle's fast flicker, a hollow resonance, a crooked choir a fifth below, a 48 ms box echo |
| Ancient Grove | mystical: a vast soft halo with shimmer, a choir an octave and a fifth above, heavy spectral blur, slow filter movement |
| **Caverns · Cave Mouth** | big rock (4.5 s, 55 ms pre-delay), discrete echoes from deep inside (290 ms, wide), drips, a draught |
| Crystal Cavern | everything rings: crystal resonances (E5, 4.5 s), very bright glassy echoes, drips |
| Underground Lake | watery: a resonant low-pass that wavers, wow, spectral drift, long dark echoes, the Endless Void (8 s), many drips |
| Dragon's Hoard | massive and dark: a sub-octave choir and a held drone, a deep resonance, saturation, a huge dark hall (7 s), glints of gold (crystal, D6) above it |

Macros: Dungeon Depth/Damp/Echo/Dread; Castle Hall Size/Grandeur/Distance; Cathedral
Vastness/Choir/Bells/Gloom; Haunted Forest Mist/Whispers/Wind/Shadows; Caverns
Depth/Drips/Echo/Rumble. Each maps to 1–6 parameters (`Source/World/Locations.cpp`).

The other five locations got clearer **sketches** where the distinctness test needed
them (Foothills: almost dry with one long echo; Mountain Pass: echo-dominated;
Observatory: the Crypt's Rising mode, a swelling reverse halo). Their design is 2b.

### Distinctness (group `locations`, gated)

A **fingerprint** of each place, from one render: 3 s of plucks, 3 s of a saw chord,
silence, a 330 Hz sine, silence, and three clicks. 23 features in dB-like units, each
with a floor below which differences are not audible:

- octave-band balance of the chord (8 bands);
- tail levels 0.2–0.6 s and 1–2 s after the chord;
- stereo width;
- flutter (10 ms level spread);
- pluck smear;
- impulse-response energy in six windows (0–20 ms … 1–2 s), averaged over the three clicks;
- harmonics, new pitches and pitch wobble around the sine (half weight: they move
  with the random modulation of some layers);
- noisiness (spectral flatness).

The distance is Euclidean (dB-equivalent).

- **Scale:** the most sensitive designed place moves by **9.0** when only its
  Intensity drops from 100 % to 90 %.
- **Gate:** any pair involving a place of the five designed locations must be at
  least **10** apart, further than that small step.
- **Two small misses** after two design rounds are **report-only**:
  Castle Courtyard ↔ Crystal Cavern (9.7) and Final Boss Lair ↔ Underground Lake
  (9.9). Both are far apart by ear (an open slap against ringing glass; a distorted
  choir against a watery wobble). The fingerprint has no feature for "metallic
  ringing" or "choir" as such.

**Closest pairs in the world** (all 40 places, current build):

| Distance | Pair | Largest differing feature |
|---|---|---|
| 6.2 | Village · Village Green ↔ Village · Market Square | tail 0.2–0.6 s (3.1) |
| 7.0 | Village · Tavern ↔ Village · Market Square | 8 kHz (4.0) |
| 7.7 | Village · Village Green ↔ Village · Tavern | 8 kHz (5.2) |
| 9.7 | Castle · Courtyard ↔ Caverns · Crystal Cavern | tail 1–2 s (5.7) (report-only) |
| 9.8 | Village · Market Square ↔ Village · Blacksmith | harmonics (6.9) |
| 9.9 | Dungeon · Final Boss Lair ↔ Caverns · Underground Lake | new pitches (6.6) (report-only) |
| 10.0 | Cathedral · Bell Tower ↔ Caverns · Crystal Cavern | IR 20–80 ms (5.2) |
| 10.2 | Cathedral · Nave ↔ Mountains · Mountain Pass | tail 0.2–0.6 s (4.6) |
| 10.3 | Castle · Courtyard ↔ Cathedral · Bell Tower | IR 0.5–1 s (5.4) |
| 10.4 | Cathedral · Cloister ↔ Caverns · Cave Mouth | pluck smear (4.7) |

The Village pairs are 2b sketches (reported, not gated); 2b must bring them to 10.

### Musicality (group `musicality`, gated)

Four stand-ins, each a 4 s phrase at −18 dBFS RMS, looped for 8 s and measured over
the last 4 s:

- guitar: Karplus-Strong plucked strings, an Am–F–C–G arpeggio;
- synth: a two-saw pad, the same chords;
- piano: stretched partials with per-partial decay, chords and a melody;
- vocal: a sung "ah", a glottal saw with vibrato through three formants, a melody
  with breaths.

They run through all 22 designed places (88 renders). Gates per render:

| Gate | Threshold | Result |
|---|---|---|
| Finite, below 0 dBFS | | yes |
| Level | within 4 dB | −0.4 … +1.0 dB |
| The instrument stays recognisable: best correlation with the aligned input over 0–2 ms lags | > 0.25 | 0.27 (Tower Top, synth: a thin, distant place by design) … 0.92 |
| No boom (63 Hz band vs the dry) | < +9 dB | up to +8.4 dB (Dragon's Hoard, guitar: the rumble is the point) |
| No fizz (8 kHz band vs the dry) | < +9 dB | up to +4.5 dB |
| The tail falls away after the phrase | > 12 dB down at the end | yes |

### Listening renders

`DungeonCrawlerTests --listen <dir>`, run in the Linux CI job into
`build/test-logs/listening` (artifact `DungeonCrawler-linux-test-logs`), 44.1 kHz
16-bit stereo, ~92 MB:

- `00-dry.wav`: the phrase dry, 4 s of guitar, then 4 s of synth, then 3 s of silence
  (the tail);
- `01-Dungeon-Corridors.wav` … `40-WizardsTower-Observatory.wav`: the same phrase
  through every place;
- `journey-2a.wav`: the guitar phrase looping while travelling (1.5 s) through the 22
  places of 2a, 4 s in each.

Measured locally: every place within ±1 dB RMS of the dry file, peaks −2.7 to −5.3 dBFS.

### The AU harness's Location check

It compared the Village with the Cathedral; with the new scenes they differ by only
1.4 dB for a sine (the harness needs 3). It now compares the Castle (Courtyard,
−13.8 dB residual) with the Dungeon (Corridors, +2.9 dB). These are the most
different pair for its sine, among all locations. The `smoke` group replays it.

### Tests

AddressSanitizer + UBSan (groups smoke, parameters, state, layouts, transparency,
place, travel, modules, realtime, editor): no findings. The only failure was the
editor's 30 fps repaint timing, which is meaningless under the sanitizer: 106 ms
there, 15 ms in the normal build.

1040 checks on Linux (full suite in three shards, 2.5 min on 4 cores). New groups:
`musicality`, and the gated `locations` fingerprint. CPU (one core of this container,
48 kHz stereo, report only): 3.0–7.0 % per location; travelling every second 9–11 %.
