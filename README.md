# DUNGEON CRAWLER

Two dark-fantasy plug-ins for macOS (AU and VST3; GarageBand and Logic Pro), built from
this one repository. A painted map of a dark realm under a red moon is their interface.

- **DUNGEON CRAWLER** (the instrument, `aumu`/`Dgci`): a software instrument played from
  MIDI (a keyboard or the piano roll). **Every place has its own instrument**, played
  inside that place's space and effects: a contrabass pizzicato in the Dungeon's
  corridors, a lute in the Castle courtyard, church bells in the bell tower, a water
  kalimba by the underground lake, a ghost violin in the graveyard, a celestial pad in
  the observatory… Choosing a place chooses its instrument and its space together;
  **Lock instrument** keeps the instrument while you travel (the Tavern's guitar in the
  Final Boss Lair).
- **DUNGEON CRAWLER FX** (the effect, `aufx`/`Dgcr`): the location effect, exactly as
  before: whatever runs through it (guitar, synth, piano, voice) sounds as if it were
  played *there*. Its Audio Unit codes and parameters are unchanged, so projects that
  used the effect (when it was called DUNGEON CRAWLER) keep working.

Both share:

- **10 locations, 40 places:** Dungeon, Castle, Cathedral, Haunted Forest, Caverns,
  Village, Swamp, Mountains, Necropolis, Wizard's Tower, each with its own scene and
  2–4 sub-locations (Treasure Room, Final Boss Lair, Bell Tower, Witch's Hut, Crystal
  Cavern, Tavern, Sunken Ruins, Frozen Peak, Mausoleum, Observatory…).
- **Every place sounds different:** its own room, and a **signature effect** no other
  place uses (iron bars ringing in the Prison Cells, a glittering octave sparkle in the
  Treasure Room, a sub-octave choir in the Boss Lair, a church bell in the Bell Tower,
  canyon echoes in the Mountain Pass, a reverse swell in the Graveyard…).
- **Smooth travel:** instant, seconds or tempo-synced; no clicks, level-matched, reverb
  and echo tails ring on.
- **Journeys and roaming:** a route of places over bars, or the plug-in wandering a
  location by itself; deterministic, synced to the song.
- **Atmosphere** (optional, off by default): sparse, quiet sounds of the place: torches,
  drips, wind, crows, a distant bell, a far-off growl.
- **Painted art:** the map and all 40 places are painterly dark-fantasy scenes built in
  Blender and painted in layers: the view zooms in with parallax, mist drifts, embers
  rise, windows flicker, the moon's glow breathes. Your own art can replace any scene
  ([docs/ART_SOURCES.md](docs/ART_SOURCES.md)).

Status: the brief ([docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md)) and Direction Updates
[1](docs/DIRECTION_UPDATE_1.md), [2](docs/DIRECTION_UPDATE_2.md) and
[3](docs/DIRECTION_UPDATE_3.md) (the instrument edition); development log and decisions in
[docs/DEVLOG.md](docs/DEVLOG.md).

## Install (macOS 10.15+, Apple Silicon and Intel)

1. Download `DungeonCrawler-macOS-Universal` from the latest green
   [CI run](../../actions/workflows/build.yml) (Artifacts) and unzip it.
2. Quit GarageBand and Logic. In Terminal type `bash ` (with a space), drag
   `install-mac.sh` from the unzipped folder into the window and press Return.
3. Wait for "DUNGEON CRAWLER and DUNGEON CRAWLER FX are installed and validated."

The package holds both plug-ins (`DungeonCrawler.component` / `.vst3`, the instrument;
`DungeonCrawlerFX.component` / `.vst3`, the effect); the script installs and validates
both. An older install's `DungeonCrawler.component` was the effect: it is replaced by the
instrument, and the effect comes back as `DungeonCrawlerFX.component` with the same codes.

The script is needed because the build is not signed with an Apple Developer ID: it
removes the browser's quarantine flag, re-signs for your Mac, refreshes the Audio Unit
list and runs Apple's validator. Details and a manual alternative:
[packaging/INSTALL.txt](packaging/INSTALL.txt). Uninstall: `bash uninstall-mac.sh`
(`--purge` also removes your presets).

## The sound (Direction Update 2)

Every place now transforms the instrument obviously at the defaults (Intensity 100 %,
Mix 100 %); Intensity 50 % is still clearly audible, 0 % is the dry signal. Spaces are a
main voice: most places use one of 33 generated spaces (huge halls with slow bloom,
reversed swells, caverns with far echoes and coin glints, small warm rooms). Levels stay
matched between places. The places the developer liked are unchanged or changed only as
noted ([docs/DEVLOG.md](docs/DEVLOG.md), Session 6).

## The instrument (DUNGEON CRAWLER)

**GarageBand:** make a Software Instrument track; in its instrument slot (Smart Controls
(B) → Plug-ins, or the Library) choose Audio Units → Veil Audio → DUNGEON CRAWLER.
**Logic Pro:** a Software Instrument track's Instrument slot → AU Instruments → Veil
Audio → DUNGEON CRAWLER. Play your MIDI keyboard or the piano roll.

- The map and the places work as in the effect: **choosing a place chooses its
  instrument and its space**. A note held while you travel keeps sounding (the space
  morphs around it; the next note plays the new place's instrument).
- **Instrument** (in the controls, where the effect has Input) slides up the instrument
  panel: the instrument's name and place, **Lock instrument**, a chooser for any of the
  forty (choosing one locks it), its five macros (Tone, Attack, Release, Body and its own
  signature control; their names follow the instrument: Pluck, Strike, Ring, Glint,
  Course…), **Intensity** (how strongly the place's effects apply) and **Space** (the
  place against the dry instrument), the pitch-bend range, the wheels and the pedal as
  they arrive, and an on-screen keyboard that lights up with the notes (click it to
  play).
- Playing: velocity (level and tone), sustain pedal, pitch bend (±2 semitones by
  default), the mod wheel (vibrato, brightness, tremulant, swell or growl, as fits the
  instrument; the panel says which), up to 4–12 notes at once depending on the
  instrument, click-free voice stealing.
- **Presets** (toolbar): *Instruments* (all forty, each in its own place with its macros
  set), *Instruments elsewhere* (locked instruments in other places: the Tavern guitar in
  the Boss Lair, church bells in the Crystal Cavern…) and the effect's *Journeys* (the
  instrument changes with each place of the route).
- Intensity 0 % plays the instrument dry; Bypass too.

| Location (family) | Its places' instruments |
|---|---|
| Dungeon (dark orchestral) | Contrabass Pizzicato · Cello Drone-Lead · Iron Chains · Golden Celesta · Doom Brass & Choir*† |
| Castle (royal court) | Lute* · Brass Fanfare · Royal Strings · Chamber Organ · Heroic Horn |
| Cathedral (sacred voices, bells) | Soft Choir* · Pipe Organ · Monk Chant* · Church Bells |
| Haunted Forest (eerie wood, wind) | Wooden Flute · Cello Harmonics · Music Box* · Ancient Harp* |
| Caverns (stone, crystal, water) | Stone Marimba · Crystal Bells† · Water Kalimba*† · Hoard Gong |
| Village (weary folk) | Tin Whistle · Weary Guitar* · Fiddle* · Tuned Anvil |
| Swamp (murky) | Detuned Banjo* · Murky Reed · Drowned Bells† |
| Mountains (highland, stone) | Shepherd's Flute · War Drums* · Ice Glockenspiel · Dwarven Chant* |
| Necropolis (death, ghosts) | Dark Choir Pad*† · Ghost Violin* · Tolling Bell & Organ* |
| Wizard's Tower (arcane) | Harpsichord Arpeggio · Clavichord · Glass Marimba · Celestial Pad*† |

\* the developer's own recordings (guitar, violin, kalimba, a soft synth pad; since Session 8
also his voice, banjo, two djembes and piano: see the DEVLOG's Session 8 for which plays
what); † carries one of the developer's effect captures (shimmer, dark hall, chorus
reverb). The rest are WORLDBUILDER's physical, modal, formant and FM models. Every
recorded instrument has a synthesised stand-in for builds without the recordings.
- DUNGEON CRAWLER FX sings with the developer's voice in a few places (the Final Boss
  Lair's sub-octave choir, the Chapel's choir, a faint bloom in the Cloister) and whispers
  faintly in the Graveyard, the Ancient Grove and the Crypt (and in their Atmosphere).
- Without MIDI the instrument is silent. MIDI travel (an effect feature) is off in the
  instrument: its notes are the music.

## The effect (DUNGEON CRAWLER FX)

**GarageBand:** select a track → Smart Controls (B) → Plug-ins → an empty Audio Effect
slot → Audio Units → Veil Audio → DUNGEON CRAWLER FX. Double-click the slot to open the
window (if you see plain sliders, switch the window's view menu from "Controls" to
"Editor"). **Logic Pro:** an Audio FX slot → Audio Units → Veil Audio → DUNGEON CRAWLER FX.

1. The window opens on the **map**. Hover a landmark to see its name; click it: a red
   path shows the journey and the view zooms into the place.
2. Inside a location, the **rings** (and the list, top right) are its places: click one
   to travel there. The breadcrumb (top left) leads back: "‹ Map" returns to the map
   without changing the sound; the location's name travels back to its own scene.
3. Automate **Location** and **Sub-location** to move through the world over the song;
   or use a **Journey** (below).

## Controls

| Control | What it does |
|---|---|
| **Character macros** (3–4, named per location) | The location's own controls, e.g. Dungeon: Depth, Damp, Echo, Dread; Castle: Hall Size, Grandeur, Distance; Swamp: Murk, Wobble, Damp. 0 % = as designed. |
| **Input** | How hard the place is driven (saturation, resonances); loudness stays matched. |
| **Intensity** | How much of the place: 0 % is the dry signal exactly, 100 % as designed. |
| **Mix** | Dry/wet. |
| **Output** | Output level. |
| **Travel** (time + sync) | How long a journey takes: Instant to 10 s, or 1/4 note to 4 bars at the song's tempo. |
| **Atmosphere** | Level of the place's sparse background sounds; 0 % = off (default). |
| **Journey** (button) | The Journey panel: route, roaming, MIDI travel, atmosphere density/seed (below). Closes with ✕, a second click on Journey, Esc or a click outside it. |
| **Bypass** | Bypass (crossfaded). |
| **Advanced** | (Closes like the Journey panel.) Every module parameter as an offset from the place (0 % = as designed) or an override ("Place" = as designed), in nine groups: I Abyss (filters, Tomb resonator), II Ancient Blood (saturation, age), III Choir, IV Wraith (spectral), V Echoes, VI Crypt (reverb), VII Relics (torch flicker, metal, wind, drips), VIII Signature (the place's signature effect: kind, amount, tone, rate), IX Vault (the place's space: one of 33 generated convolution spaces, mix, shimmer, tone, duck), X Ensemble (chorus, rising phaser, tape echo with pitch per repeat, water drops). Offsets travel with you. |
| **Preset name ▾, ◀ ▶** | Init / No preset (everything at its default, no journey), factory presets (favourite spots, journeys), your presets, Save as your preset… After any change the name shows a star ("Grand Castle Tour *"). |
| **Journey active · Stop** (top centre) | Shown while a Journey or Roam runs (journey presets start one). Stop ends it where it is. |
| **A / B, A→B** | Compare two complete settings; copy the current one to the other slot. |
| **↶ ↷** | Undo / redo of your edits in the window. |
| **⚙** | Window size (75–200 %, or drag the corner) and Quality: Eco (lightest: no animation, mono spectral layer), Standard, High (smoothest animation). |
| **▾** (above the controls) | Hide the controls to see the whole picture. |

**Journey panel:**

| Control | What it does |
|---|---|
| **Journey** + mode | Follow the route: Loop, One-way or Ping-pong. |
| **Bars per Stop** | How long each stop lasts (1–16 bars). |
| **Add this place / Remove last / Clear** | Build the route (up to 8 stops) from the places you visit. The map shows it with numbered stops. |
| **Roam** + rate | The plug-in wanders between the current location's places, every ½–16 bars. |
| **Randomness / Seed** | 0 % visits the places in order; more is more random. The same seed always gives the same wandering at the same bar. |
| **MIDI travel** + octave | Notes C–A of the chosen octave pick the ten locations, C–E an octave higher the places (see host support below). |
| **Atmosphere Density / Seed** | How often background sounds come, and which. |

**A click always wins:** choosing a location on the map, a place in the list or a
hotspot, or the location in the breadcrumb, takes you there at once and stops a running
Journey or Roam (a short note says "Journey stopped"; turn it on again in the panel or
reload the preset). Host automation of Location does not stop a journey: the journey
steers while it is on.

Journeys, roaming and the atmosphere follow the song's position when it plays (the same
bar always gives the same result) and keep moving at the song's tempo when it is
stopped.

**MIDI travel, host support (honestly):** the plug-in is an audio effect (`aufx`) so
that it loads in GarageBand. GarageBand does not send MIDI to audio effects, and Logic
only offers side-chain MIDI to "MIDI-controlled effects", so MIDI travel will usually do
nothing there: use Journey, Roam or automation instead. VST3 hosts that route MIDI to
effects (e.g. Reaper, Cubase, Bitwig) work.

## Troubleshooting

| Problem | Fix |
|---|---|
| Silent slot with plain sliders in GarageBand | The quarantine flag: run `install-mac.sh` again (from the unzipped folder), restart GarageBand. |
| Plug-in missing from the list | GarageBand: Settings → Audio/MIDI → tick "Audio Units", restart. Logic: Plug-in Manager → Reset & Rescan. |
| Plain sliders instead of the map | The window's view menu: "Editor", not "Controls". |
| "travelling…" for a moment | A scene is prepared the first time it is shown (a fraction of a second; engraved scenes about a second at Retina size); after that it is cached. |
| CPU too high on a laptop | ⚙ → Quality → Eco. |
| Something else | Run `bash diagnose-mac.sh` (in the unzipped folder) and send the report it writes to the Desktop. |

Your own art can replace any scene: see [docs/ART_SOURCES.md](docs/ART_SOURCES.md).
Manual test checklist: [docs/MANUAL_TEST.md](docs/MANUAL_TEST.md).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # fetches JUCE 8.0.16, or pass -DJUCE_PATH=...
#   add -DDC_SAMPLES_DIR=build/samples after preparing the developer's recordings:
#   python3 tools/sample-prep/prepare.py --source ../Samples --out build/samples
cmake --build build --parallel                   # both plug-ins: DungeonCrawler (instrument), DungeonCrawlerFX (effect)
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests            # the test suite
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --calibrate Source/World/LevelTrims.h   # after sound changes
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen build/listening               # WAV renders of every place
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --art build/art 960                   # PNG of the map and every scene
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --signatures                          # how much each signature changes its place
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --calibrate-instruments Source/Instrument/Trims.h   # the instruments' level trims
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen-instruments build/instruments   # one melody on every instrument
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen-recordings build/recordings     # the instruments with the new recordings; the FX's voice places before/after
```

The instrument's sampled instruments use the developer's recordings (the private
`AdrianAbbas/Samples` repository, prepared by `tools/sample-prep`); CI fetches them with the
`SAMPLES_TOKEN` secret. They are never committed here; without them those instruments
play synthesised stand-ins. Recordings may sit in subfolders and be MP3 or WAV; a new one
needs an entry in `tools/sample-prep/manifest.json`. The FX ships only the three voice sets
its places use.

On macOS this builds a universal (arm64 + x86_64) AU, VST3 and Standalone of each
plug-in. CI (`.github/workflows/build.yml`) builds both, runs the full test suite,
validates both with `auval` and an AU host harness each (the effect's and the
instrument's; in- and out-of-process, arm64 and x86_64), packages both together, and
renders the listening WAVs.

## Layout

| Path | What |
|---|---|
| `Source/DSP/` | The modules: from VEIL (Abyss, Ancient Blood, Choir, Wraith, Echoes, Crypt) and new (Relics, Signature, Atmosphere, Vault + Convolver, Ensemble) |
| `Source/World/` | Locations, sub-locations, macros, signatures (`Locations.*`), level trims, journeys (`Journey.h`), factory presets (`Presets.h`) |
| `Source/Engine.*` | The chain, travel, crossfades, the reverb rooms and signature pools, level matching |
| `Source/UI/` | The painted scenes (layers, manifest), the engraving renderer for the rest, the background scene cache, the look |
| `tools/painted/` | The Blender + painting pipeline that makes the painted art ([README](tools/painted/README.md)) |
| `assets/` | The painted layers (`painted/`, shipped in the bundles) and the fonts (OFL) |
| `Source/Instrument/` | The instrument engine: WORLDBUILDER's voice models (`wb/`) and sample bank, the voice pool and MIDI (`Synth.*`), the forty instruments (`Designs.cpp`), level trims |
| `Source/PluginProcessor.*`, `Source/PluginEditor.*` | One processor for both plug-ins (effect or instrument; journeys, MIDI, presets, A/B, undo) and window; `Source/UI/InstrumentPanel.*` the instrument's panel |
| `tools/sample-prep/` | Prepares the developer's recordings for the sampler (from WORLDBUILDER) |
| `Tests/` | Offline tests through `processBlock` and the editor |
| `tools/au-harness/`, `scripts/`, `packaging/` | CI host harnesses (effect, instrument), install/uninstall/diagnostics, install notes |
