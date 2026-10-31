# DUNGEON CRAWLER

A dark-fantasy **audio effect** (AU + VST3, macOS: GarageBand and Logic Pro). Choose a
place in a fantasy world and whatever runs through the plug-in sounds as if it were
played there; travelling between places morphs the sound.

**Status: Milestones 1 and 2a.** The place engine: ten locations, each with its own
scene and its sub-locations (40 places), smooth travel with a Travel Time (instant,
seconds or tempo-synced), crossfades for what cannot morph, reverb tails that ring
on, calibrated level matching, 3–4 themed macros per location and an Advanced drawer.
The sound design of the Dungeon, Castle, Cathedral, Haunted Forest and Caverns is done;
the other five locations are sketches until Milestone 2b. The interface is still a
simple one; the map and the illustrations follow (see
[docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) and [docs/DEVLOG.md](docs/DEVLOG.md)).

## Install (macOS 10.15+, Apple Silicon and Intel)

Download `DungeonCrawler-macOS-Universal` from the latest green
[CI run](../../actions/workflows/build.yml) (Artifacts), unzip it and follow
`INSTALL.txt`: in short, run `bash install-mac.sh` from that folder. The script is
needed because the build is not signed with an Apple Developer ID (it removes the
browser's quarantine flag, re-signs for your Mac and validates the plug-in).

In GarageBand: Smart Controls → Plug-ins → an Audio Effect slot → Audio Units →
Veil Audio → DUNGEON CRAWLER. Manual checks: [docs/MANUAL_TEST.md](docs/MANUAL_TEST.md).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # fetches JUCE 8.0.16, or pass -DJUCE_PATH=...
cmake --build build --parallel
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests
# after changing a place's sound: re-measure the level trims
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --calibrate Source/World/LevelTrims.h
# listening renders: the same guitar and synth phrase through every place
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen build/listening
```

On macOS this builds a universal (arm64 + x86_64) AU, VST3 and Standalone. CI
(`.github/workflows/build.yml`) builds, tests, validates with `auval` and an AU host
harness (in- and out-of-process, arm64 and x86_64), and packages.

## Layout

| Path | What |
|---|---|
| `Source/DSP/` | The six modules from VEIL (Abyss, Ancient Blood, Choir, Wraith, Echoes, Crypt), Relics (new), and their parameters |
| `Source/World/` | The world: locations, sub-locations, macros (`Locations.*`) and calibrated level trims (`LevelTrims.h`) |
| `Source/Engine.*` | The chain, travel between places, crossfades, the reverb rooms, level matching |
| `Source/PluginProcessor.*`, `Source/PluginEditor.*`, `Source/UI/` | Plug-in and interface |
| `Tests/` | Offline tests through `processBlock` |
| `tools/au-harness/` | AudioToolbox host used by CI |
| `scripts/` | CI, packaging, install, uninstall, diagnostics |
