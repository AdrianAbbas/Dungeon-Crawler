# DUNGEON CRAWLER — Direction Update 1 (overnight plan: Milestones 6–12)

Read after DUNGEON_CRAWLER_BRIEF.md. This file overrides it where they conflict and **replaces the remaining milestones (2b, 3, 4, 5)** with Milestones 6–12 below.

Status: Milestones 0, 1 and 2a are done and tested in GarageBand. Developer feedback: places still sound too similar and don't match their vibe yet. A full sound redesign with reference audio comes in a later session; tonight, push distinctness as far as possible with the rules in Milestone 6.

## Overnight rules (read first)

- **Unattended run. Complete Milestones 6 → 12 in order without stopping between them or for approval.** Don't ask questions; make reasonable decisions and record them in `docs/DEVLOG.md`.
- Put the whole plan into the DEVLOG checklist first (one line per milestone and sub-part). Update it after every part. After a usage limit or compaction, resume from the first unticked item.
- Push after each finished part with `[skip ci]`; push **without** `[skip ci]` after each whole milestone so builds exist in the morning.
- Small metric misses after 2 attempts → report-only, noted. Never leave the build broken. Don't wait for CI. `runs-on: macos-15`. Never put `: ` inside an unquoted YAML step name.
- Editor: double-buffered, cached layers, ≤ 30 fps, never blocks the message thread, never flickers or freezes a MacBook Air. Audio: real-time safe, click-free, level-matched, no forced pitch glides.
- Final handoff: what was done, what to listen for and look at in GarageBand per milestone, known issues.

---

## Milestone 6 — Distinct places (sound pass) + remaining locations

- Build **Village, Swamp, Mountains, Necropolis, Wizard's Tower** with all sub-locations.
- **Signature rule:** every place (all locations and sub-locations, including the 2a ones) gets **one signature effect no other place uses**, on top of its acoustic character, so any two places are tellable apart blind. Examples to start from:

| Place | Signature |
|---|---|
| Dungeon — Entrance Hall | stone corridor flutter + distant torch-crackle-coloured early reflections |
| Dungeon — Prison Cells | metallic **bar resonance** (tuned comb), tight muffled cell |
| Dungeon — Treasure Room | **glittering pitched shimmer** (octave/fifth sparkle on transients) |
| Dungeon — Final Boss Lair | **sub-octave choir + menace drive**, vast dark tail, slow ominous delay |
| Castle — Great Hall | wide stone hall with banners (diffuse, warm), long early-reflection spread |
| Castle — Throne Room | **regal brass-like resonance** emphasis, grand but controlled |
| Castle — Chapel | sacred **choir pad** bloom under the instrument |
| Castle — Tower Top | **wind-blown** open air: sparse long echoes, gentle movement |
| Cathedral — Nave | the longest gothic reverb, majestic |
| Cathedral — Crypt | close, low, **dark low-mid stone resonance** |
| Cathedral — Bell Tower | **tuned bell resonators** ringing with the input |
| Haunted Forest — Old Path | damped open space, **eerie spectral blur** |
| Haunted Forest — Witch's Hut | small wooden room + **bubbling, crooked pitch wobble** |
| Haunted Forest — Ancient Grove | **mystic shimmer** with slow-breathing modulation |
| Caverns — Crystal Cavern | **bright crystal resonances** (glassy modal ring) |
| Caverns — Underground Lake | **underwater muffle** with slow wobble + long watery tail |
| Caverns — Dragon's Hoard | massive rumble, **low growl saturation** |
| Village — Tavern | small wooden room, **warm saturation**, slight crowd-room smear |
| Village — Market Square | open air with **slap-back off buildings** |
| Village — Blacksmith | **anvil-metal resonance**, gritty |
| Swamp — Bog | **murky low-passed wobble**, damp, thick |
| Swamp — Sunken Ruins | half-submerged: filtered, **sloshing modulation** |
| Mountains — Mountain Pass | **long canyon echoes** (discrete, receding) |
| Mountains — Frozen Peak | thin, cold, airy: **icy high shimmer**, sparse echoes |
| Mountains — Dwarven Hall | deep stone hall, **forge-warm low resonance** |
| Necropolis — Graveyard | cold, sparse, **ghostly reverse-swell** |
| Necropolis — Mausoleum | tight stone tomb, **muffled hollow** |
| Wizard's Tower — Library | dry, hushed, **paper-soft damping** |
| Wizard's Tower — Alchemy Lab | **bubbling pitch modulation**, strange resonances |
| Wizard's Tower — Observatory | vast **starry shimmer**, slow evolving |

- Musical first: guitar, synth, piano and vocal must stay usable in a mix everywhere.
- Update the distinctness test (all pairs; list the closest) and the listening render (same guitar + synth phrase through every place).

## Milestone 7 — The engraved map

- Full-window **engraved fantasy map** home screen (Section 2 of the brief): hatched mountains, forests, coastline, castle, dungeon entrance, caves, swamp, village, graveyard, wizard's tower, compass rose, decorative cartouche, dark parchment texture, blood-red accent for the active place.
- Each landmark clickable with hover name; the current location marked; a path line drawn when travelling.
- Clean transitions map → location (zoom/fade), double-buffered, no stale frames.
- Art sources per the brief (documented public-domain engravings in `docs/ART_SOURCES.md` and/or rendered engraving-style art; drop-in override folder).

## Milestone 8 — Location and sub-location scenes

- Full-window engraved illustrations for **every location and every sub-location**, matching the map's style.
- Sub-locations clickable inside each location scene (doors, stairs, points of interest) + a compact list; breadcrumb/back control.
- Subtle life: torch flicker glow, drifting fog/dust, candle shimmer, water glints where fitting — cached and cheap.
- Snapshots of every scene to `docs/snapshots/`.

## Milestone 9 — Journeys and performance

- **Journey path:** chain places into a route that plays over a set number of bars (loop or one-way), tempo-synced, automatable.
- **Roam mode:** the plugin slowly wanders between a location's sub-locations on its own (rate, randomness, seed; deterministic).
- **MIDI travel (optional input):** if the host sends MIDI to the effect (sidechain MIDI where supported), notes on a chosen octave select places; document host support honestly (GarageBand may not route MIDI to effects; Logic can).
- Travel Time in seconds or bars; all travel parameters automatable; travel morphs stay click-free and level-matched.

## Milestone 10 — Atmosphere layer

- Optional **Atmosphere** layer per place: subtle, sparse one-shot or bed sounds that fit (torch crackle, dripping water, distant wind, chains, a far-off growl in the boss lair, bubbling in the alchemy lab, crows in the graveyard). Synthesised, original, **off or very low by default**, its own level and density controls, never busy, never risers or noise waves.
- It must not mask the instrument; tempo-aware density; deterministic with a seed.

## Milestone 11 — Workflow and quality

- Preset browser (factory "favourite spots" and "journeys", user presets), **A/B compare**, undo/redo for edits.
- Resizable window (fixed aspect, integer-friendly scales), Eco/Standard/High quality modes (reverb density, oversampling, animation detail).
- Full state restoration (project save/reopen, duplicate instance), stable parameter IDs, versioned state.
- CPU report on the MacBook-Air-class budget (report-only).

## Milestone 12 — Release polish

- Factory presets (at least 20: favourite spots + journeys), README (what it is, install, GarageBand/Logic usage, every control, troubleshooting), `docs/MANUAL_TEST.md` GarageBand checklist, install docs.
- Final QA pass: distinctness report, level matching, click checks on every travel, no stuck animation, editor CPU.
- Final DEVLOG handoff.
