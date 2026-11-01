# DUNGEON CRAWLER — Direction Update 3: Instrument edition

Read after DUNGEON_CRAWLER_BRIEF.md and Direction Updates 1–2. This file overrides them where they conflict.

## 0. The change

DUNGEON CRAWLER becomes **two plugins built from this same repository**:

1. **DUNGEON CRAWLER** (new, the main product): a **software instrument** (AU type `aumu`, new subtype, e.g. `Dgci`; VST3 instrument). Played from MIDI (keyboard or the host's piano roll). **Every sub-location has its own unique instrument**, played *inside* that sub-location's space and effects.
2. **DUNGEON CRAWLER FX**: the current effect plugin (`aufx` / `Dgcr`), renamed, otherwise unchanged in behaviour. Existing projects using the effect keep working (keep its AU codes and parameter IDs).

Both share the place engine, map, art, navigation, effects/captures, presets system and tests. CI builds, validates (auval + host harness: instrument and effect) and packages **both**; the install script installs both.

**Scope (important):** this is a **normal, single-instrument plugin** — one instrument playing at a time per plugin instance, one instance per track, played from the host's MIDI/piano roll or a keyboard, like a typical software instrument. **Not** a WORLDBUILDER-style multi-instrument workstation: no pattern editor, no arrangement, no generated parts, no several instruments playing at once. From WORLDBUILDER, reuse only the **sound engine** (sampler/synthesis) and its **Instrument-mode** MIDI playing and keyboard view.

## 1. How the instrument works

- **Same map and zoom navigation.** Choosing a sub-location selects **its instrument and its space** together. Travel morphs the space smoothly; a note held during travel keeps sounding (no hung notes, no clicks).
- **Lock instrument** toggle: keep the current instrument while travelling to other places, to hear e.g. the Tavern lute in the Final Boss Lair.
- **Instrument panel** (slide-up drawer, same painterly dark style): instrument name and sub-location, 4–6 macros that fit the instrument (e.g. Tone, Attack, Release, Body, Space, plus one signature control), Intensity of the place's effects, an on-screen keyboard lighting up with incoming notes, pitch bend/mod wheel/sustain shown.
- **Playing:** velocity (level and tone), sustain pedal, pitch bend (±2 st default), mod wheel mapped musically per instrument; sensible polyphony; click-free voice stealing. No forced pitch glides.
- Presets: instrument presets (instrument + place + macros) plus the existing journeys.

## 2. Engine — reuse, don't reinvent

Reuse **WORLDBUILDER's instrument engine** from `AdrianAbbas/Worldbuilder` (read access; copy, never modify that repo): its sampler (key mapping, velocity layers, round-robins, loops, articulations by note length), physical/modal/formant/FM synthesis models, Instrument-mode MIDI handling and keyboard view. Use the developer's own recordings in the private `AdrianAbbas/Samples` repo wherever they fit (classical guitar, violin incl. pizzicato and textures, kalimba, banjo, Minilogue patches, pedal captures) — process them so each instrument has its own identity. Fetch samples in CI with the existing secret; never commit them to the public repo.

## 3. Instruments (one per sub-location; each location is a themed family)

All instruments in a location share a **family theme**; each must still be clearly distinct, dark-realm in mood, and sound good dry *and* in its space. Adjust names to the sub-locations that actually exist in the code.

| Location (family theme) | Sub-location → instrument |
|---|---|
| **Dungeon** (dark orchestral: low strings, iron, gold) | Entrance Hall → low bowed **cello** drone-lead · Corridor → **contrabass pizzicato** · Prison Cells → **bowed/struck iron chains** (metallic) · Treasure Room → **golden celesta** with coin-like glints · Final Boss Lair → colossal **doom brass + choir stabs** |
| **Castle** (royal court) | Courtyard → **lute** (guitar samples) · Great Hall → **brass fanfare** section · Throne Room → regal **string ensemble** · Chapel → small **chamber organ** · Tower Top → heroic **horn** calls |
| **Cathedral** (sacred voices & bells) | Nave → grand **pipe organ** · Cloister → soft **choir** (voice, formant/samples) · Crypt → low **monk chant** · Bell Tower → tuned **church bells** |
| **Caverns** (stone, crystal, water) | Cave Mouth → firelit **stone marimba** · Crystal Cavern → **glass/crystal bells** (+ shimmer capture) · Underground Lake → **water kalimba** (kalimba samples) · Dragon's Hoard → huge **tuned gong / growling low brass** |
| **Village** (weary folk) | Village Green → **recorder / tin whistle** · Tavern → **guitar** (guitar samples, weary and warm) · Market Square → **fiddle** (violin samples) · Blacksmith → tuned **anvil** percussion |
| **Haunted Forest** (eerie wood & wind) | Forest Edge → detuned **wooden flute** · Old Path → creaking **cello harmonics** · Witch's Hut → alluring **music-box lullaby** · Ancient Grove → ancient **harp** with breathy whispers |
| **Swamp** (murky) | Bog → murky low **reed** (bass-clarinet-like) · Mire → detuned **banjo** (banjo samples) · Sunken Ruins → **underwater bells** / glassy pad |
| **Mountains** (highland & stone) | Foothills → **shepherd's flute** · Mountain Pass → **war drums** · Frozen Peak → **ice glockenspiel** · Dwarven Hall → **dwarven chant + low brass** |
| **Necropolis** (death and ghosts) | Gates → dark **choir pad** · Graveyard → **ghost violin** (violin samples + textures) · Mausoleum → **tolling bell + organ** |
| **Wizard's Tower** (arcane) | Stairwell → ascending **harpsichord arpeggio** instrument · Library → soft **clavichord** · Alchemy Lab → bubbling **glass marimba** synth · Observatory → celestial **synth pad** (Minilogue soft pad) |

## 4. Milestones (one long run, in order)

1. **Two-plugin setup:** instrument + FX targets, identities, CI for both (auval + harness for an instrument too), install script, README. Keep FX behaviour intact.
2. **Instrument engine:** port WORLDBUILDER's engine, MIDI handling, keyboard view, instrument panel, Lock instrument, travel with held notes.
3. **Instruments, first half:** Dungeon, Castle, Cathedral, Caverns, Village.
4. **Instruments, second half:** Haunted Forest, Swamp, Mountains, Necropolis, Wizard's Tower.
5. **Presets and polish:** at least 30 instrument presets, tests (every instrument plays from MIDI, in tune, distinct from its siblings, level-matched; no hung notes during travel; both plugins validate), listening render (the same short melody on every instrument, dry and in its place), DEVLOG handoff with what to listen for.

**Rules:** never modify the Worldbuilder, LIMINAL or Light-Seeker repos (copy what you need). DEVLOG checklist (one line per milestone part and per instrument), resume from it after a limit. Push after each part with `[skip ci]`, without after each whole milestone. Small metric misses after 2 attempts → report-only; never leave the build broken. Don't wait for CI; don't ask questions. `runs-on: macos-15`. Never put `: ` inside an unquoted YAML step name. Editor double-buffered, cheap, never freezing a MacBook Air. No brand names in the product.
