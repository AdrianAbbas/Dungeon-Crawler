# DUNGEON CRAWLER

A dark-fantasy **audio effect** for macOS (AU and VST3; GarageBand and Logic Pro). An
engraved map of a dark realm is its interface: choose a place and whatever runs
through the plug-in (guitar, synth, piano, voice) sounds as if it were played *there*.
Travelling between places morphs the sound smoothly, and the window shows an engraved
illustration of where you are.

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
- **Engraved art:** a full-window map and an illustration for every place, drawn in an
  antique engraving style, with subtle life (flickering torches, drifting fog).

Status: Milestones 0–12 of [docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) and
[docs/DIRECTION_UPDATE_1.md](docs/DIRECTION_UPDATE_1.md); development log and decisions
in [docs/DEVLOG.md](docs/DEVLOG.md).

## Install (macOS 10.15+, Apple Silicon and Intel)

1. Download `DungeonCrawler-macOS-Universal` from the latest green
   [CI run](../../actions/workflows/build.yml) (Artifacts) and unzip it.
2. Quit GarageBand and Logic. In Terminal type `bash ` (with a space), drag
   `install-mac.sh` from the unzipped folder into the window and press Return.
3. Wait for "DUNGEON CRAWLER is installed and validated."

The script is needed because the build is not signed with an Apple Developer ID: it
removes the browser's quarantine flag, re-signs for your Mac, refreshes the Audio Unit
list and runs Apple's validator. Details and a manual alternative:
[packaging/INSTALL.txt](packaging/INSTALL.txt). Uninstall: `bash uninstall-mac.sh`
(`--purge` also removes your presets).

## Using it

**GarageBand:** select a track → Smart Controls (B) → Plug-ins → an empty Audio Effect
slot → Audio Units → Veil Audio → DUNGEON CRAWLER. Double-click the slot to open the
window (if you see plain sliders, switch the window's view menu from "Controls" to
"Editor"). **Logic Pro:** an Audio FX slot → Audio Units → Veil Audio → DUNGEON CRAWLER.

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
| **Journey** (button) | The Journey panel: route, roaming, MIDI travel, atmosphere density/seed (below). |
| **Bypass** | Bypass (crossfaded). |
| **Advanced** | Every module parameter as an offset from the place (0 % = as designed) or an override ("Place" = as designed), in nine groups: I Abyss (filters, Tomb resonator), II Ancient Blood (saturation, age), III Choir, IV Wraith (spectral), V Echoes, VI Crypt (reverb), VII Relics (torch flicker, metal, wind, drips), VIII Signature (the place's signature effect: kind, amount, tone, rate). Offsets travel with you. |
| **Preset name ▾, ◀ ▶** | Factory presets (favourite spots, journeys), your presets, Save as your preset… |
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
| "engraving…" for a moment | The art is drawn the first time a scene is shown (about a second at Retina size); after that it is cached. |
| CPU too high on a laptop | ⚙ → Quality → Eco. |
| Something else | Run `bash diagnose-mac.sh` (in the unzipped folder) and send the report it writes to the Desktop. |

Your own art can replace any scene: see [docs/ART_SOURCES.md](docs/ART_SOURCES.md).
Manual test checklist: [docs/MANUAL_TEST.md](docs/MANUAL_TEST.md).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # fetches JUCE 8.0.16, or pass -DJUCE_PATH=...
cmake --build build --parallel
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests            # the test suite
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --calibrate Source/World/LevelTrims.h   # after sound changes
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen build/listening               # WAV renders of every place
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --art build/art 960                   # PNG of the map and every scene
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --signatures                          # how much each signature changes its place
```

On macOS this builds a universal (arm64 + x86_64) AU, VST3 and Standalone. CI
(`.github/workflows/build.yml`) builds, runs the full test suite, validates with
`auval` and an AU host harness (in- and out-of-process, arm64 and x86_64), packages,
and renders the listening WAVs.

## Layout

| Path | What |
|---|---|
| `Source/DSP/` | The modules: from VEIL (Abyss, Ancient Blood, Choir, Wraith, Echoes, Crypt) and new (Relics, Signature, Atmosphere) |
| `Source/World/` | Locations, sub-locations, macros, signatures (`Locations.*`), level trims, journeys (`Journey.h`), factory presets (`Presets.h`) |
| `Source/Engine.*` | The chain, travel, crossfades, the reverb rooms and signature pools, level matching |
| `Source/UI/` | Engraving renderer, motifs, the map and the 40 scenes, the background scene cache, the look |
| `Source/PluginProcessor.*`, `Source/PluginEditor.*` | Plug-in (journeys, MIDI, presets, A/B, undo) and window |
| `Tests/` | Offline tests through `processBlock` and the editor |
| `tools/au-harness/`, `scripts/`, `packaging/` | CI host harness, install/uninstall/diagnostics, install notes |
