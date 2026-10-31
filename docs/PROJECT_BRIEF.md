# DUNGEON CRAWLER — Claude Code Master Brief

**DUNGEON CRAWLER** is a dark-fantasy **audio effect** plugin (AU + VST3, macOS, GarageBand and Logic Pro). The user travels a **fantasy map**: choosing a **location** (Dungeon, Castle, Haunted Forest…) and then a **sub-location** inside it (Treasure Room, Final Boss Lair…) reshapes whatever instrument runs through the plugin so it sounds like it's being played *there* — and the interface changes to an illustration of that place.

It replaces the developer's earlier plugin **VEIL** (repo `AdrianAbbas/Dark-Fantasy-Plugin`). Keep VEIL's sound engine as raw material; **discard VEIL's interface and its central "Ritual Engine" sigil entirely.**

---

## 0. How to work (read first)

**Setup.** Claude Code runs in the cloud from the developer's PC. The developer's MacBook Air (fanless, Apple Silicon) only tests finished builds in GarageBand. All macOS building, signing and testing runs on GitHub Actions. Usage is limited; be efficient.

**Sources to reuse:**
- **Build pipeline:** copy from `AdrianAbbas/Worldbuilder` — CMake structure (pinned JUCE 8.x, `AU_SANDBOX_SAFE TRUE`, universal arm64 + x86_64, low deployment target), `.github/workflows/` (Linux full tests in shards; macOS universal build, smoke test, auval, AU host harness on arm64 + x86_64, packaging, ccache, report-only CPU metrics), `scripts/install-mac.sh`, `uninstall-mac.sh`, `tools/au-harness/`. Adapt the harness for an **effect** (audio in → audio out). Read Worldbuilder's `docs/DEVLOG.md` lessons first.
- **DSP:** copy the useful modules from `AdrianAbbas/Dark-Fantasy-Plugin` (VEIL): filters (Abyss), cathedral/FDN reverb (Crypt), spectral/freeze (Wraith), saturation/degradation (Ancient Blood), delays incl. reverse/descent (Echoes), pitch choir (Choir), plus any later improvements. Audit them; keep what sounds good, fix or replace what doesn't.
- New AU identity: type `aufx`, e.g. subtype `Dgcr`, manufacturer `Veil`, its own bundle ID and product name "DUNGEON CRAWLER".

**Working rules (every session):**
- Work milestone by milestone; split big ones into sub-milestones across sessions.
- Push after each finished part with `[skip ci]`; the **final push of a session has no `[skip ci]`**.
- Keep a progress checklist at the top of `docs/DEVLOG.md`; resume from it after a compaction or usage limit.
- Small metric misses after 2 attempts → report-only; never leave the build broken.
- Don't wait for CI; don't ask questions when unattended; record decisions in the DEVLOG.
- CI failures: read only the last ~100 lines; max 2 fix attempts, then explain plainly.
- Never put `: ` inside an unquoted YAML step name.
- **Editor drawing:** double-buffered, cached, ≤ 30 fps, never blocks the message thread, never flickers or freezes a MacBook Air.
- **Audio:** real-time safe, no clicks on any change, level-matched (switching location must not jump loudness), no forced pitch glides.

---

## 1. The experience

1. **The map (home screen).** A full-window, hand-engraved-style **fantasy map**: mountains, forests, a castle, a dungeon entrance, a cave, a swamp, a coast, a graveyard… each location a clickable landmark. The currently active location is marked. Hovering a landmark names it.
2. **Choosing a location** smoothly **travels** there: the sound morphs to the location's effect chain over a short travel time, and the view transitions (zoom/fade) from the map to a **full-window illustration of that location**.
3. **Inside a location,** its **sub-locations** appear as clickable points or doors in the illustration (and in a short list). Choosing one morphs the sound and swaps the illustration again (e.g. Dungeon → Treasure Room).
4. A breadcrumb / back control returns to the location, then to the map. Travelling between any two places always morphs smoothly.
5. **Location and sub-location are automatable**, so a composer can move through the world over the timeline.

## 2. Visual style

Mood reference: monochrome **antique engraving / woodcut / etching** illustration — dense line hatching, charcoal blacks, dusty greys, a single blood-red accent; gothic blackletter title; clean readable labels. (The developer's reference image comes from another plugin: **mood only — never copy it or ship it.**)

- **Map:** engraved-cartography look — hatched mountains, forests as clustered tree glyphs, coastline hatching, a compass rose, decorative cartouche, parchment-dark paper texture. Original artwork only.
- **Location and sub-location scenes:** full-window engraved illustrations (castle on a hill, dungeon corridor with torches, treasure vault, boss lair with a looming silhouette, crypt, forest…), same style as the map.
- **Sources for the art (pick what works, document it):**
  1. **Public-domain 19th-century engravings** (e.g. Gustave Doré and similar illustrators whose works are long out of copyright). Use only images whose public-domain status is clearly documented on their source (e.g. Wikimedia Commons with a PD licence tag); record each image's source and licence in `docs/ART_SOURCES.md`; crop, combine and grade them into scenes.
  2. **Procedural/rendered engraving style:** build scenes in Blender and render with a non-photoreal engraving/hatching look (line art + hatching shader/halftone lines), or generate hatching procedurally from depth/lighting.
  3. **Drop-in folder** `assets/scenes/<location>/<sublocation>.png` so the developer can replace any scene with commissioned or own art later.
- **Subtle life:** flickering torchlight glow (red/amber accent), drifting dust or fog, slowly moving hatching shadows — cached and cheap.
- **Controls** float minimally over the art: thin, elegant sliders and knobs in the engraved style (like the reference: tall slim faders with ivory caps and a red value track), a blackletter title, readable sans-serif labels.

## 3. Sound design per place

Each **location** defines a base effect character; each **sub-location** refines it. Built from the VEIL modules plus additions as needed (e.g. muffling/low-pass "through stone", tremolo torch flicker, metallic resonators, underwater filtering, dark choir, bell resonances, distance/early-reflection models, gentle noise beds for wind or dripping only where fitting and always subtle).

Starting world (Claude Code may refine names and add/remove where sensible; 8–10 locations, 3–4 sub-locations each):

| Location | Sub-locations | Sound idea |
|---|---|---|
| **Dungeon** | Entrance Hall · Prison Cells · Treasure Room · Final Boss Lair | stone reverb, damp darkness; cells = tight, muffled, metallic bars; treasure = bright glittering reflections, shimmer; boss lair = huge, menacing, low choir, saturation, slow delays |
| **Castle** | Great Hall · Throne Room · Chapel · Tower Top | grand halls; regal; chapel = long sacred reverb + choir; tower = windy, open, distant |
| **Cathedral / Abbey** | Nave · Crypt · Bell Tower | very long gothic reverb; crypt = dark, close, low; bell tower = metallic bell resonances |
| **Haunted Forest** | Old Path · Witch's Hut · Ancient Grove | damped open space, eerie spectral blur; hut = small wooden room; grove = mystical shimmer |
| **Caverns** | Crystal Cavern · Underground Lake · Dragon's Hoard | crystal = bright resonant; lake = watery, filtered, long; hoard = massive, dark, rumbling |
| **Village** | Tavern · Market Square · Blacksmith | tavern = warm wooden room; square = open, slight slap echo; smithy = metallic, gritty |
| **Swamp** | Bog · Sunken Ruins | murky low-passed, wobbling, damp; ruins = underwater-ish muffled echoes |
| **Mountains** | Mountain Pass · Frozen Peak · Dwarven Hall | pass = long open echoes; peak = cold, thin, airy; dwarven hall = stone, deep, resonant |
| **Necropolis** | Graveyard · Mausoleum | cold, sparse, ghostly; mausoleum = tight stone tomb |
| **Wizard's Tower** | Library · Alchemy Lab · Observatory | dry hushed library; lab = bubbling modulation, strange pitch; observatory = vast starry shimmer |

Rules:
- Every place must sound **clearly different** — switching places should transform the instrument dramatically, tellable blind.
- **Musical first:** a guitar, synth, piano or vocal through any place must still sound good and usable in a mix.
- Level-matched across places; smooth morphs (parameters interpolate; incompatible states crossfade); no clicks; reverb tails continue naturally when moving.

## 4. Controls

- **Main (always visible, few):** Mix (dry/wet), Intensity (how strongly the place's character applies), Input, Output, and **3–4 macro sliders per location** with themed names (e.g. Dungeon: *Depth*, *Damp*, *Echo*, *Dread*; Castle: *Hall Size*, *Grandeur*, *Distance*), each mapped to several underlying parameters.
- **Travel Time** (instant → several seconds or tempo-synced) and **Bypass**.
- **Advanced drawer** (hidden by default): the full module parameters for power users.
- Presets (factory "journeys" and favourite spots), state saving, host automation of location and sub-location.

## 5. Milestones

0. **Setup + audit:** new repo pipeline from Worldbuilder, VEIL DSP audited and imported, minimal effect skeleton that passes audio in GarageBand with a basic location switch. Push and **stop** for a GarageBand check.
1. **Place engine:** location/sub-location data model, smooth morphing and travel, automation, level matching, the macro system, Advanced drawer (simple UI for now).
2. **Sound design:** 2a first five locations with all sub-locations; 2b the rest. Listening render: the same guitar/synth phrase through every place.
3. **Art pipeline + prototype:** the map plus Dungeon (all sub-locations) and one more location in the engraved style, with transitions and controls. Snapshots to `docs/snapshots/`. **Stop for approval.**
4. **Full art** for all locations and sub-locations; polish animations.
5. **Presets, polish, docs and release** (README, manual test checklist, install).
