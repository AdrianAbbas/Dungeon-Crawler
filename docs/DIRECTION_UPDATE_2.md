# DUNGEON CRAWLER — Direction Update 2: Sound redesign from listening notes

Read after DUNGEON_CRAWLER_BRIEF.md and Direction Update 1. This file overrides them where they conflict.

The developer listened to every place in GarageBand. Below: the global problem, then per-place notes (developer's words summarised, plus the technical direction). **Keep everything marked LIKE exactly as it is or only strengthen it slightly; rework everything marked CHANGE.**

---

## 1. Global: the effects are far too weak

**Biggest change for every single place:** many places sound like no effect is active at all. Fix this everywhere:
- At default settings (Intensity 100%, Mix 100%), every place must transform the input **obviously** — a listener should instantly hear "I'm somewhere else", not a subtle filter.
- Rebalance each place's internal wet/dry, reverb sends, delay feedback, modulation depth and character effects upward. Use the reverb/space as a main voice, not a garnish.
- Intensity should scale from subtle (low) to dramatic (100%); 50% must already be clearly audible.
- Still level-matched (no big loudness jumps between places) and musical (guitar/synth/piano/vocal stay usable).
- **Use the developer's own pedal captures** in the private `AdrianAbbas/Samples` repo (Black Hole Symmetry, EQD Astral Destiny, Boss RE-202, OBNE Visitor, Polychrome — sweep sequences, possibly reamped phrases and "static" versions). Build convolution IRs and matched models from them (see LIMINAL's capture work if already done there; reuse it). Some takes contain crackles/hiss: detect and repair or exclude damaged parts, and report files to re-record. Never use pedal brand names in the product.
- Add a **"strength" check** to the tests: for each place at default, the processed output must differ strongly from the dry input (spectral/temporal distance and wet-energy ratio above a clear threshold); report the weakest places.

## 2. Per-place notes

### Necropolis
- **Mausoleum — LIKE.** Unique, good filtering. Keep.
- **Gates — CHANGE.** Wider, darker, deeper; mystery and intrigue. → wide dark stereo reverb, low-mid weight, slow ominous modulation, distant dark echoes.
- **Graveyard — CHANGE.** Morph the sound into something sinister and mysterious that makes you uneasy. → cold detuned/pitch-unstable layer, ghostly reverse-swell reverb, sparse eerie delays, slight dissonant shimmer below the input.

### Mountains
- **Foothills — CHANGE.** Barely any effect. Something unique, a bit fluttery as sound travels from hill to hill. → multi-tap echoes panned across the stereo field with flutter/slight wow, open air reverb, gentle distance filtering on repeats.
- **Mountain Pass — CHANGE.** Massive; sound clamours between the walls of the pass. → loud, big reverb plus **tape delay** (RE-202 capture) bouncing between left/right walls (ping-pong slap with tape darkening), high feedback but controlled.
- **Frozen Peak — CHANGE.** Sharper, frozen, jagged, like hitting crystals. → strong **low cut** leaving mostly highs, crystalline resonances/bright modal ringing, icy high shimmer, glassy short reflections.
- **Dwarven Hall — LIKE the filter, CHANGE the space.** It only sounds like a filter. → keep the filter, add a big deep stone-hall reverb with forge warmth and resonant low-mid stone body so you can imagine the hall.

### Cathedral
- **Cloister — LIKE** (the shimmer). Boost the effect overall for a larger change.
- **Crypt — LIKE** (the filters). Add more mystery and a sinister vibe → **reverse reverb**, dark swells.
- **Nave — CHANGE.** (The nave is the long main hall of a cathedral, where the congregation stands.) Barely any effect. → the **longest, most majestic reverb** in the plugin: huge, smooth, stone, with long pre-delay and slow bloom (Astral Destiny plain hall or Black Hole capture).
- **Bell Tower — CHANGE.** You should imagine a bell being struck: its ringing and feedback. → **tuned bell resonators** excited by the input (inharmonic bell partials, hum/strike tones), ringing feedback, some metallic swirl.

### Caverns
- **Underground Lake — LIKE. Near perfect. Keep exactly.**
- **Cave Mouth — CHANGE** (too weak). More enclosed; fire flickering between the walls of the entrance as an adventurer readies himself. → enclosed rocky early reflections, medium cave tail, subtle flicker modulation (tremolo-like light flicker) on the reflections, warm.
- **Crystal Cavern — CHANGE.** Doesn't sound right. Shimmery and magical, a room full of crystals; much stronger. → **sampled Astral Destiny octave shimmer** + bright crystal resonators + glassy sparkle.
- **Dragon's Hoard — CHANGE.** Right idea, wrong execution; too similar to Underground Lake. Think of the scene where a hobbit sneaks into a sleeping dragon's lair inside the mountain: the vast cavern, the echoes when something falls. → **enormous** dark cavern with long distinct far echoes, deep low rumble, a metallic "coin" sheen on reflections, slow heavy movement — clearly different from the lake (dry and vast, not watery).

### Dungeon
- **Corridor — LIKE** (decent for now).
- **Prison Cells — LIKE** the sound. **The water droplets sound very unrealistic** → remove them, or rebuild as realistic sparse drops (proper water-drop synthesis with a stone tail), very quiet.
- **Entrance Hall — CHANGE** (strengthen).
- **Treasure Room — CHANGE.** Bland; sounds like the dry synth. Rich, like sound reflecting off gold coins. → rich bright golden reverb, lush chorus/ensemble, glittering pitched shimmer on transients, metallic high sheen.
- **Final Boss Lair — CHANGE.** Right idea; make it **huge and sinister**, like you're about to fight the devil himself. → colossal dark reverb (Black Hole capture), sub-octave choir layer, menacing drive/saturation, slow ominous delays, pitch-down shadow layer.

### Castle (all CHANGE — currently bland)
- **Courtyard:** fluttery and mystical → open stone courtyard reverb with fluttery multi-taps and gentle shimmer/modulation.
- **Great Hall:** **MASSIVE** → huge hall reverb, long, wide, grand.
- **Throne Room:** rich → chorus/ensemble modulation, regal warm reverb, slight harmonic richness.
- **Chapel:** quieter, intimate, like a sermon in progress → softer, warmer, close, sacred small-chapel reverb with a gentle choir bloom, slightly muffled.
- **Tower Top:** open, looking over the world ahead → wide airy open space, long distant echoes, minimal reflections, bright.

### Village (all CHANGE — the whole village is weary; we're in the dark realm)
- **Village Green:** calming modulation that softens the sound.
- **Tavern:** calm, almost empty, only a few weary travellers → small warm wooden room, soft, slightly dull, gentle saturation.
- **Market Square:** slow, dead, darker → dark, damped, sparse slap echoes, slowed/dragging modulation.
- **Blacksmith:** the only loud space in the village; anvils reflect a lot of sound → strong metallic resonance (anvil/metal ring), bright hard reflections, gritty drive.

### Haunted Forest
- **Old Path — LIKE. Sinister, perfect. Keep.**
- **Witch's Hut — LIKE**, make it a little more **alluring**, enticing → sweet, warm, seductive shimmer/chorus over the eerie wobble; inviting on the surface, wrong underneath.
- **Forest Edge — CHANGE.** On edge: you know you shouldn't enter but have to. → dark pitch shifter/modulator (slight downward pitch shadow, unstable detune), tense filtering.
- **Ancient Grove — CHANGE.** Boring and bland; design something memorable (e.g. ancient, magical, breathing: slow evolving shimmer, wooden resonance, whispering spectral blur).

### Swamp
- **Bog — LIKE. Just right.**
- **Sunken Ruins — LIKE.** Like Bog but more shimmery; reads as water, not mud. Keep.
- **Mire — CHANGE.** A bit bland → darker and more reflective (dark wet reflections, longer murky tail).

### Wizard's Tower
- **Alchemy Lab — LIKE**, but **lower the flanger rate** and add more effects alongside it (bubbling pitch modulation, strange resonances).
- **Stairwell — CHANGE.** Boring → an **ascending modulator** (rising phaser/pitch spiral, spiral-staircase echoes climbing upward).
- **Library — CHANGE.** Quieter and filtered precisely like a library → hushed, dry, paper-soft damping, small close space, gentle high roll-off.
- **Observatory — CHANGE.** More shimmer, stronger → vast starry shimmer (Astral capture), long and bright.

---

## 3. Process and rules

- Split across sessions if needed; keep a DEVLOG checklist (one line per place), and resume from it after a limit.
- For each place, render the same guitar and synth phrase dry vs processed (test-logs artifact) and run the strength check; list the weakest ten places at the end.
- Don't change the LIKE places except where noted.
- Push after each location with `[skip ci]`; the final push of the session without it. Small metric misses after 2 attempts → report-only. Don't wait for CI. `runs-on: macos-15`. Never put `: ` inside an unquoted YAML step name.
