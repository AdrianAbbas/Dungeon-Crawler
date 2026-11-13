# Manual test (GarageBand on a Mac)

## M0: does it pass audio and switch locations?

Install (once per build):

1. Download `DungeonCrawler-macOS-Universal` from the latest green CI run
   (Actions → Native build → Artifacts) and unzip it.
2. Quit GarageBand. In Terminal: `bash ` (with a space), drag `install-mac.sh` from the
   unzipped folder into the window, press Return. Wait for
   "DUNGEON CRAWLER is installed and validated."

In GarageBand:

1. Open a project with a track that plays something (a software instrument with a
   recorded region, or an audio loop).
2. Select the track, open Smart Controls (B) → Plug-ins → an empty Audio Effect slot →
   Audio Units → Windward Audio → DUNGEON CRAWLER.
3. Play. Check, and note anything that differs:
   - [ ] The plug-in window shows the DUNGEON CRAWLER interface (title, ten places,
         four faders). If it shows plain sliders, switch the window's view menu from
         "Controls" to "Editor".
   - [ ] Sound comes through, in the Dungeon (dark, stony reverb).
   - [ ] Clicking another place changes the sound over about 1.5 s, with no click,
         dropout or jump in loudness. Try several, also while a journey is under way.
   - [ ] Intensity at 0 sounds like the dry track; Mix at 0 too; Bypass too.
   - [ ] Output changes the level; Input changes how hard the place is driven.
   - [ ] Close and reopen the plug-in window: the marked place is still the active one.
   - [ ] Save the project, close it, reopen it: the place and the faders are restored.
   - [ ] Playback stays smooth with the window open (no stutter or freeze on the Air).

If something fails, download `scripts/diagnose-mac.sh` from the repository, run
`bash diagnose-mac.sh` and send the report it writes to the Desktop.

## M1 + M2a: places, travel, macros (after session 2)

Install the new build as above. In GarageBand, on a guitar, synth, piano or vocal track:

- [ ] The window shows three columns: the ten locations; the chosen location's places
      (its own scene first, then its sub-locations) with "Location › Place" above
      them, and its macros; then Input, Intensity, Mix, Output and Travel.
- [ ] Clicking a location arrives at its scene; clicking a sub-location travels there.
      Every place of the Dungeon, Castle, Cathedral, Haunted Forest and Caverns sounds
      clearly different, and the instrument stays usable (not buried, no boom, no fizz).
- [ ] Travel: drag Travel to "Instant" (fully left), then try 5 s, then choose
      "1 bar" in the menu with the project tempo set. The morph takes that long, with
      no click, dropout or jump in loudness.
- [ ] Treasure Room → Final Boss Lair while a chord sustains: the sparkle above fades
      out, then the low choir fades in (no pitch glide, no click).
- [ ] Play a chord in the Cathedral's Nave, stop, and click the Village's Tavern
      straight away: the Nave's long tail keeps ringing out instead of being cut.
- [ ] Macros: each location shows its own names (Dungeon: Depth, Damp, Echo, Dread;
      Castle: Hall Size, Grandeur, Distance). Each one audibly changes the sound;
      double-click returns it to 0 %.
- [ ] Advanced opens a drawer with every module parameter. Moving one changes the
      sound; "Place" or 0 % is as designed. Close it again.
- [ ] Automation: in the track's automation lanes, Location and Sub-location can be
      drawn, and playback moves through the places.
- [ ] Intensity at 0 sounds exactly like the dry track.
- [ ] Save, close and reopen the project: location, place, macros, travel settings and
      Advanced values are restored.
- [ ] Playback stays smooth with the window open (no stutter or freeze on the Air).

Listening without GarageBand: the CI artifact `DungeonCrawler-linux-test-logs` has
`listening/`: the same guitar and synth phrase dry (`00-dry.wav`) and through every
place, plus a journey file (the guitar travelling through the places).

## Direction Update 2: the sound redesign (session 6)

Play the same guitar or synth phrase through each place at the defaults (Intensity
100 %, Mix 100 %), then at Intensity 50 %. Reference renders: the CI artifact
`DungeonCrawler-linux-test-logs`, folder `listening/`, one `NN-...-dry-vs-processed.wav`
per place (dry first, then the place).

- [ ] Every place sounds obviously somewhere else at 100 %; 50 % is still clearly audible.
- [ ] Switching places never jumps in loudness and never clicks; tails ring on.
- [ ] Kept exactly as before: Mausoleum, Underground Lake, Corridors, Old Path, Bog, Sunken Ruins.
- [ ] Kept, with the note applied: Dwarven Hall (same filter, now a big deep stone hall), Cloister (boosted), Crypt (notes drawn in backwards), Prison Cells (old drips gone; rare quiet drops), Witch's Hut (sweeter shimmer and chorus on top), Alchemy Lab (slower wobble, bubbles, stranger resonance).
- [ ] Necropolis: Gates wide, dark, deep with far dark echoes; Graveyard uneasy (reversed swells, a tritone shimmer below, wavering echoes).
- [ ] Mountains: Foothills answers flutter from hill to hill; Pass massive with a tape echo bouncing left/right, darker each time; Frozen Peak sharp, thin, crystalline.
- [ ] Cathedral: Nave the longest, most majestic space; Bell Tower: you strike a bell, it rings and swirls.
- [ ] Caverns: Cave Mouth enclosed rock with flickering firelight; Crystal Cavern shimmering octave and crystals; Dragon's Hoard enormous, dry-dark, far echoes, coin sheen (clearly not the lake).
- [ ] Dungeon: Entrance Hall stronger; Treasure Room rich and golden; Final Boss Lair colossal, sinister, falling echoes.
- [ ] Castle: Courtyard fluttery and mystical; Great Hall massive; Throne Room rich; Chapel quiet and intimate; Tower Top open and airy with distant echoes.
- [ ] Village (weary): Green calming; Tavern small, warm, dull; Market dead and dark; Blacksmith loud metal.
- [ ] Haunted Forest: Forest Edge tense with a dark pitch shadow; Ancient Grove breathing, magical.
- [ ] Swamp: Mire darker, more reflective. Wizard's Tower: Stairwell rising forever; Library hushed; Observatory vast starry shimmer.
- [ ] CPU on the MacBook Air stays comfortable with two or three instances (the Vault adds a convolution).

## M6: distinct places (after the overnight session)

Every place now has a **signature**: one effect no other place uses. On a guitar or
synth track, walk through all 40 places (location list, then each place) and listen for:

- [ ] **Dungeon:** Corridors: footstep-like reflections receding down a passage ·
      Entrance Hall: a fluttering buzz between stone walls, crackly early reflections ·
      Prison Cells: metal bars ringing with the notes, muffled · Treasure Room: a
      glittering octave sparkle on every attack · Final Boss Lair: a low "oh" choir an
      octave down, distorted, slow repeats.
- [ ] **Castle:** Courtyard: a quick left-right ping-pong · Great Hall: soft, wide,
      swaying early reflections · Throne Room: a brassy resonance · Chapel: an "ah"
      choir pad swelling under the notes · Tower Top: two long echoes wavering in the wind.
- [ ] **Cathedral:** Cloister: a rapid picket of column echoes · Nave: long cascades from
      the vault · Crypt: a dark low-mid stone hum · Bell Tower: a church bell rung by
      every attack.
- [ ] **Haunted Forest:** Forest Edge: scattered, drifting echoes · Old Path: notes
      smeared into a ghostly wash · Witch's Hut: a crooked pitch wobble · Ancient Grove:
      a shimmer an octave up that breathes in and out slowly.
- [ ] **Caverns:** Cave Mouth: a deep hum and a far answer · Crystal Cavern: glassy
      ringing on attacks · Underground Lake: underwater, wobbling · Dragon's Hoard: a
      growling, snarling low end.
- [ ] **Village:** Green: a doubled, chorused sound · Tavern: warm, softly saturated,
      cosy · Market Square: a hard slap-back · Blacksmith: an anvil ringing on attacks,
      gritty.
- [ ] **Swamp:** Mire: soft, dragged attacks · Bog: murky and wobbling · Sunken Ruins:
      filtered, sloshing.
- [ ] **Mountains:** Foothills: almost dry, one long answer from the right · Mountain
      Pass: five echoes receding · Frozen Peak: a thin icy twinkle two octaves up ·
      Dwarven Hall: a warm low hum.
- [ ] **Necropolis:** Gates: an iron groan on attacks · Graveyard: notes swelling in
      backwards · Mausoleum: hollow, tubey, muffled.
- [ ] **Wizard's Tower:** Stairwell: echoes circling around you (headphones) · Library:
      hushed and soft · Alchemy Lab: bubbling pitch and strange resonances ·
      Observatory: twinkling starlight high above.
- [ ] Any two places are tellable apart with your eyes closed (ask someone to click).
- [ ] Guitar, synth, piano and vocal stay usable everywhere (not buried, no boom, no fizz).
- [ ] Travelling between any two places (Travel 1.5 s, and Instant): no click, no dropout,
      no jump in loudness; the old place's echoes ring out instead of stopping.
- [ ] Advanced drawer: the new "Signature" group (Signature, Amount, Tone, Rate) changes
      the signature; Amount −100 % removes it.

## Hotfixes 1–3 (after the GarageBand test of the painted build)

- [ ] **Stuck in a preset (1):** load "Grand Castle Tour". A "Journey active" plate with
      Stop appears at the top. Click the Map, then the Dungeon: you go there at once, the
      plate disappears, a note says "Journey stopped", the preset reads
      "Grand Castle Tour *". Play for a minute: you stay in the Dungeon.
- [ ] Load it again; inside the Castle click another place (list or ring): same.
- [ ] Load it again; press **Stop** on the plate: the sound and picture stay where the
      journey was.
- [ ] Preset menu → **Init / No preset**: name "Init", no plate, the Dungeon, defaults.
- [ ] **Journey panel (2):** press Journey (bottom right): the panel opens on the left.
      Close it four ways: ✕, Journey again, Esc, a click on the picture. Advanced the same.
      Bypass, Advanced and the ▾ button stay clickable with a panel open.
- [ ] **Swamp (3):** load "Swamp Wander" ("Roam active"). Click Bog, Sunken Ruins, Mire
      in the list and on the rings: each one takes effect, "Roam stopped". With the
      Journey panel open, the place list (top right) is still clickable.

## Visual overhaul: the painted map and all 40 places

Painted art for the map and every place (the engraved scenes below are only the fallback
now).

- [ ] The window opens on a **painted map**: a dark land seen from high above, lava
      rivers, fog banks, the red moon rising behind a jagged range, each location a
      glowing landmark with its **name** underneath in small capitals. The current
      location has a **crimson glow** and its name in crimson.
- [ ] **Life:** embers rise from the lava and fires, mist drifts slowly across the low
      ground, the moon's glow breathes, windows flicker. Gentle, never busy.
- [ ] Click the Dungeon or the Castle: the path is drawn, then the **zoom**: the map's
      layers rush past at different speeds (clouds fastest) while the place settles in,
      also in layers (**parallax**). Same feel and timing as before.
- [ ] Inside: Corridors (lava crack, torches, the glowing arch of the Lair at the end),
      Entrance Hall (the great gate onto the moonlit mountains), Prison Cells (bars,
      chains, the cells glowing red), Treasure Room (gold in a shaft of red light), Final
      Boss Lair (the horned, winged lord over a lava lake); Courtyard (the castle on its
      crag under the moon), Great Hall, Throne Room (the rose window), Chapel (candles,
      tall windows, light shafts), Tower Top (the keep against the moon, the sea of mist).
- [ ] Rings (hotspots) sit on the doors, stairs and features of each picture; the list
      top right and the breadcrumb work as before.
- [ ] **Controls:** a dark veil instead of a box, small group headings (Character,
      Levels, Travel), slim faders with ivory caps and a crimson track, readable serif
      labels. Everything still works as before.
- [ ] The other eight locations, all their places: Cloister, Nave (rose window, pews),
      Crypt (sarcophagi), Bell Tower (the bell in the arch, the moon behind); Forest Edge,
      Old Path (wisps), Witch's Hut (crooked hut, cauldron), Ancient Grove (standing
      stones, the old tree); Cave Mouth, Crystal Cavern (red crystals), Underground Lake,
      Dragon's Hoard (the sleeping dragon); Village Green, Tavern (the hearth), Market
      Square (stalls, lanterns), Blacksmith (the forge); Mire, Bog, Sunken Ruins;
      Foothills, Mountain Pass (the canyon), Frozen Peak (above the clouds), Dwarven Hall
      (pillars, the river of fire); Gates (the iron gate), Graveyard, Mausoleum;
      Stairwell (the spiral stair), Library, Alchemy Lab, Observatory (the moon in the
      dome's slit). Rings on each picture's doors and features.
- [ ] Eco (⚙ > Quality) stops all animation; the Air stays cool with the window open.
- [ ] Optional: a folder of layers (back.png, mid.png, fog.png, near.png) in the drop-in
      folder replaces a scene with parallax ([ART_SOURCES.md](ART_SOURCES.md)).

## M7 + M8: the engraved map and the scenes (now the other locations)

Open the plug-in window (GarageBand: double-click the DUNGEON CRAWLER slot; the view
menu must say "Editor", not "Controls").

- [ ] It opens on a **full-window engraved map**: hatched mountains with snow, the sea
      on the left with hatched coastline, a river, woods, dotted roads, a compass rose,
      a cartouche with the title (red "D"). The art appears within about a second
      ("engraving the map..." shows until then).
- [ ] The current location has a **red ring** on the map. Hovering a landmark rings it
      and shows its name and how many places it has.
- [ ] Click another landmark: a **red dashed path** draws from where you were (as fast
      as the sound travels), then the view **zooms into** the place.
- [ ] Inside: a full-window illustration of the place, **rings** for its other places
      (red = where you are; names on hover), the **list** top right, and the
      **breadcrumb** top left (‹ Map · Location › Place).
- [ ] Clicking a ring or a list entry travels there; the picture **crossfades** to the
      new place. "‹ Map" zooms back out without changing the sound; the location name
      travels back to the location's own scene.
- [ ] Every one of the 40 places has its own picture (e.g. Treasure Room: gold piles,
      chests, a crown; Final Boss Lair: a horned silhouette with red eyes on a throne of
      skulls; Bell Tower: the great bell; Witch's Hut: the crooked hut and cauldron;
      Observatory: the telescope under the starry slit).
- [ ] **Subtle life:** torches and candles flicker, gold, water and stars glint, fog
      drifts in the forest, swamp and graveyard. It is gentle, never distracting.
- [ ] No flicker, no half-drawn frames, no freeze while clicking around quickly; the
      MacBook Air stays cool with the window open (Activity Monitor: GarageBand's CPU
      should not rise noticeably because of the window).
- [ ] The ▾ button (above the controls, right) hides the controls strip to show the
      whole picture; ▴ brings it back.
- [ ] Automating Location/Sub-location while the window is open: on the map the ring
      and path follow; inside a location the picture follows.
- [ ] Optional: put your own PNG in `~/Library/Windward Audio/DUNGEON CRAWLER FX/scenes/`
      (see [ART_SOURCES.md](ART_SOURCES.md)), reopen the window: it replaces that scene.

## M9: journeys, roaming, MIDI travel

Set the project to 4/4 at a tempo you like, put a looping region on the track.

- [ ] **Journey:** open "Journey" (controls strip). Visit three or four places and
      press "Add this place" in each. Turn Journey on, Bars per Stop = 2, Loop. Play:
      every two bars the sound (and the window) moves to the next stop, morphing
      smoothly (Travel 1.5 s, or set Travel to "1 bar"). The map shows the route with
      numbered stops.
- [ ] Stop and start playback from different bars: each bar always lands on the same
      stop. One-way stays at the last stop; Ping-pong goes back and forth.
- [ ] **Roam:** turn Journey off, Roam on (2 bars, Randomness 50 %): the plug-in wanders
      between the current location's places by itself. Same seed + same bar = same
      place every time; another seed wanders differently.
- [ ] No clicks, dropouts or loudness jumps at any stop change.
- [ ] Save, close, reopen the project: the route and all journey settings are back.
- [ ] **MIDI travel:** GarageBand does not send MIDI to audio effects, so this does
      nothing there (expected). In a host that does (see the README), notes C–A of
      the chosen octave pick the ten locations, C–E an octave higher the places.

## M10: Atmosphere

- [ ] By default there is **no** atmosphere (the Atmosphere slider in the controls
      strip is at 0 %). The sound is exactly as before.
- [ ] Raise Atmosphere to 50–100 % with a sparse part playing: now and then a quiet
      sound of the place appears in the background: Dungeon: drips, torch crackle,
      chains; Boss Lair: a far-off growl; Castle Courtyard: wind and crows; Chapel: a
      distant bell; Caverns: drips, glass tinkles, lapping water; Tavern: a murmuring
      room and the fire; Blacksmith: hammer clinks; Swamp: frogs and bubbles; Mountain
      Pass: wind and a distant howl; Graveyard: crows, an owl, whispers; Alchemy Lab:
      bubbling. Never busy, never a riser or a noise wave.
- [ ] Play loud: the atmosphere steps back under the instrument (it never masks it).
- [ ] Journey panel → Atmosphere: Density changes how often; another Seed changes which
      sounds come when. Playing the same bars again gives the same sounds (deterministic).
- [ ] Mix 0 %, Intensity 0 % or Bypass: the atmosphere is gone too.

## M11: workflow

- [ ] **Presets:** click the preset name (bottom left): Favourite spots, Journeys. Each
      one moves you there (journeys start moving when you play). ◀ ▶ step through them.
- [ ] "Save as your preset…", give it a name; it appears under "Your presets" and in a
      new project.
- [ ] **A/B:** set up a place on A, click B, set up another; click A and B to compare.
      A→B copies the current one to the other.
- [ ] **Undo/redo:** move a fader, click ↶: it goes back; ↷: forward again. Automation
      playback does not end up in the undo history.
- [ ] **Window size:** drag the bottom-right corner, or ⚙ → Window size (75–200 %). The
      picture stays sharp, nothing overlaps, the aspect stays. Reopen the project: same size.
- [ ] **Quality:** ⚙ → Quality → Eco: no animation, a little less CPU (Activity Monitor);
      High: smoother torchlight. The sound in Eco is nearly the same (the spectral
      ghosts are mono).
- [ ] Duplicate the track with the plug-in (GarageBand: Track → Duplicate Track): the copy
      sounds and looks identical.

## M12: release check (the short version)

A quick pass over everything, about 15 minutes, on the MacBook Air:

- [ ] Install with `install-mac.sh` from the downloaded zip; GarageBand shows the map.
- [ ] Guitar, synth, piano and voice tracks: try three favourite-spot presets each; the
      instrument always stays usable and the loudness does not jump.
- [ ] Load "Descent into the Dungeon", play 12 bars: five stops, smooth morphs, the
      window follows.
- [ ] Atmosphere 60 % in "Graveyard Mist": occasional crows and whispers, never busy.
- [ ] Resize to 150 %, switch Eco/Standard/High, undo a few edits, A/B two presets.
- [ ] Save, quit GarageBand, reopen: everything is as you left it.
- [ ] With the window open for 5 minutes during playback the Air stays cool and quiet
      (no stutter, no freeze, no flicker).

## Direction Update 3: the instrument (DUNGEON CRAWLER) and the effect (DUNGEON CRAWLER FX)

Install from the new package (`bash install-mac.sh`): it must end with both plug-ins
validated.

**The effect still works**
- [ ] An old project that used DUNGEON CRAWLER as an effect opens with the effect in
      place (now named DUNGEON CRAWLER FX) and sounds as before.
- [ ] A new Audio Effect slot lists Windward Audio > DUNGEON CRAWLER FX; it behaves as before.

**The instrument**
- [ ] A Software Instrument track: Windward Audio > DUNGEON CRAWLER loads; the painted map
      opens; without notes it is silent.
- [ ] Play a few notes: the Dungeon's Corridors play the contrabass pizzicato.
- [ ] Visit every location and a few of its places: each place plays its own instrument
      (the crumb top left names it), in its space. Listen first to: the Lute (Courtyard),
      Weary Guitar (Tavern), Fiddle (Market Square), Water Kalimba (Underground Lake),
      Ghost Violin (Graveyard) and Celestial Pad (Observatory), which are your
      recordings; then Crystal Bells (shimmer capture), Doom Brass & Choir, Church Bells,
      Pipe Organ, Harpsichord Arpeggio (hold a key: it climbs at the song's tempo).
- [ ] Hold a chord and click another place: the chord keeps sounding while the space
      morphs; the next notes play the new instrument. Let go: nothing hangs.
- [ ] The instrument panel under the world (Session 9): its five macros carry the
      instrument's names (e.g. Snap, Glint, Course, Whispers); move each, they change the
      sound. Intensity and Effects change the place's share.
- [ ] Lock instrument on, travel to the Final Boss Lair: the same instrument, in the lair.
      Choose another instrument from the panel's list: it plays and Lock is on.
- [ ] The panel's keyboard lights up with every note you play and can be clicked; the
      Bend, Mod and Pedal indicators follow your keyboard.
- [ ] Velocity, sustain pedal, pitch bend (±2 st; the panel's Bend menu changes it) and
      the mod wheel (the panel says what it does on this instrument) all respond.
- [ ] Fast playing on a bell or celesta (many notes): no clicks when notes are stolen.
- [ ] Presets (toolbar): "Instruments", "Instruments elsewhere" and "Journeys" menus;
      a few of each load and play. A journey moves the instrument with the place.
- [ ] Logic Pro: the same, plus automation of Location and Sub-location changes the
      instrument over the song.
- [ ] CPU on the Air: four instances with chords held (Activity Monitor or Logic's meter).

**Session 8: your voice, banjo, djembes and piano**
- [ ] Soft Choir (Cloister): your soft ooh/aah with the choir stacks; the Open macro
      (called Vowel until Session 9) morphs ooh to aah while a chord is held. Play above E5: the voice shifts gently and
      the synthesised choir takes over.
- [ ] Monk Chant (Crypt): your hummed and gravelly drones on low notes, an octave below
      the keys; Open: hum to chant. Dwarven Chant (Dwarven Hall): the gravel drone low,
      the aah drone higher, your grit under it; Brass adds the low brass.
- [ ] Dark Choir Pad (Gates): six of you, wide and slow, three more an octave down.
- [ ] Doom Brass & Choir (Final Boss Lair): the choir stab is your full aah/ooh stacks with
      grit; Weight brings in the low piano strike.
- [ ] Ancient Harp (Ancient Grove): Whispers turns up your whispers and breaths.
- [ ] Music Box (Witch's Hut): your soft hum under the box: more alluring, or too much?
- [ ] Detuned Banjo (Mire): soft to hard; repeat one note fast (round-robins: no
      machine-gun); Detune.
- [ ] War Drums (Mountain Pass): play across the keyboard: the big drum's bass at the
      bottom up to the small drum's muted slap at the top, pitched down and huge.
- [ ] Tolling Bell & Organ (Mausoleum): your piano struck low under the bell.
- [ ] DUNGEON CRAWLER FX on a guitar or synth: the Graveyard, the Ancient Grove and the
      Crypt whisper faintly while you play (turn Atmosphere up there too); the Final Boss
      Lair and the Chapel sing with your voice; the Cloister blooms faintly.

## Session 9: the window, the instrument panel, the Effects roller

**The window reopens as it was left** (both plug-ins)
- [ ] Go into a place (e.g. Cathedral → Crypt), open Advanced and scroll it, make the
      window bigger (corner or ⚙). Close the plug-in window, open it again: the same
      scene (not the world map), Advanced open and scrolled, the same size.
- [ ] The same with the Journey panel open; with the controls hidden (▾); on the map.
- [ ] Save the project, quit GarageBand, reopen the project: the window opens as it was.
- [ ] Load a preset: the window stays where it is (only the sound changes).
- [ ] The instrument: lock or choose an instrument, close and reopen: the same instrument.

**Hover never sticks**
- [ ] Click a place in the list (top right), then move the mouse over the other names and
      away: each glows only while the mouse is over it; the chosen place keeps its crimson
      diamond and rule. Same for the hotspots in the scene and the landmarks on the map.
- [ ] Move quickly in and out of the window over the list and the panel's buttons:
      nothing stays lit.

**The instrument panel** (DUNGEON CRAWLER)
- [ ] The panel sits under the world (no Instrument button any more): name, ‹ › and the
      chooser, Lock, Bend range; Tone…signature macros, Vowel, Ensemble, Level, Intensity,
      the Effects roller; Bend and Mod wheels, Sustain, the keyboard.
- [ ] Play from your MIDI keyboard: the keys light crimson; the wheels and Sustain follow
      your keyboard. Drag the Bend wheel (it springs back), the Mod wheel, click Sustain.
- [ ] Soft Choir (Cloister): Vowel moves ooh ↔ aah, Ensemble from one singer (0 %) to a
      full choir (100 %), for the next notes. On the Contrabass they are dimmed.
- [ ] Level: −24 to +12 dB, smooth while moving.

**The Effects roller** (instrument panel; the FX's controls next to Intensity)
- [ ] Hold a chord and roll it from 100 % down to 0 %: the place's effects fade to the
      nearly dry instrument, with no click and no jump in loudness.
- [ ] Roll it up to 150 %: more reverb, echoes, voices and character than before, about
      as loud.
- [ ] Automate it in Logic; save it in a user preset; open an older project: it is at 100 %.
- [ ] Double-click: back to 100 %. Scroll wheel moves it.

## Session 10: Search

- [ ] Both plug-ins: the magnifier (top right) and Cmd+F open the search; Esc, the
      magnifier again, or a click outside close it. Click a landmark with it open: it
      closes and you travel there.
- [ ] Type slowly "crypt": results at every letter, no lag on the Air. Try typos
      ("crpyt", "dwarvn", "kalimab") and moods ("eerie", "sacred").
- [ ] Arrow down through the results (the description follows), Enter: you land as if
      you had clicked there (map: the path and the zoom; in a scene: the morph).
- [ ] Instrument: search "war drums", Enter: the panel shows War Drums, locked. Hold a
      note while choosing another result: it rings on.
- [ ] Instrument: Hold to hear on a few instruments: a short phrase, sensible level,
      stops when you let go; with the song playing it does nothing.
- [ ] Chips: Instruments, Places, Presets, Dark, Epic, Eerie, Sacred filter the list.
- [ ] Close the search with nothing typed: Recent shows what you chose; save and reopen
      the project: still there. Close and reopen the window after a search: same place.

## Session 11: Mix, brass, Punch

- [ ] A song with three or four instances (brass, strings, choir, drums): with the new
      defaults, does it still drown? Open Mix on each and turn Reverb / Space down: the
      reverb goes, the instrument stays about as loud.
- [ ] Low Cut at 200-300 Hz on a long space (Dragon's Hoard, Nave): the mud goes and the dry
      keeps its level. Pre-delay +40 ms: the hit comes clearly before the reverb.
- [ ] Duck at 50-100 %: the tail dips under fast playing and blooms in the pauses. No pumping
      you would not want?
- [ ] Echoes at 0 % in the Mountain Pass and the Foothills: the echoes stop. Modulation at 0 %
      in the Witch's Hut: no wobble.
- [ ] Move each slider quickly while playing: no clicks. Automate one in Logic.
- [ ] Brass Fanfare, Heroic Horn, Doom Brass: the hit cuts through, soft playing is softer;
      the sustain no louder than before.
- [ ] Punch 0 / 100 / 200 % and Time 20 / 120 / 500 ms on the brass, the War Drums and a pad.
      Nothing clips at 200 % with chords.
- [ ] Open a project saved before this version: it sounds exactly as before.

## Session 12: horns that hit, Punch UI

- [ ] Heroic Horn (Tower Top), Brass Fanfare (Great Hall), Doom Brass & Choir (Final Boss Lair),
      Dwarven Chant with Brass up (Dwarven Hall): does the hit crack now, at least as hard as
      LIGHT SEEKER's Heroic Horns and Fanfare Trumpets? Too harsh anywhere?
- [ ] Hard and soft playing: soft notes blow in, hard ones bite. The held note no louder.
- [ ] Loud chords (velocity 120+), Punch at the top, Level +12 dB: nothing clips, no pumping.
- [ ] In their places, in a song: the dry hit lands before the reverb; the tail blooms
      between the notes.
- [ ] Punch and Punch Time on the instrument panel: the readout in dB follows the instrument
      (Off on the organ, +12.0 dB on the fanfare), typing "+6" or "Off" works, double-click
      resets. They look right next to Intensity?
- [ ] A project saved with the Session 11 version: the brass sounds exactly as it did and its
      Punch values are kept.
- [ ] Listen: test-logs `brass/` (each horn before / after, dry and in its place, and
      LIGHT SEEKER's with the same phrase).

## Session 13: real brass

- [ ] Heroic Horn, Brass Fanfare, Doom Brass & Choir, Dwarven Chant with Brass up, Hoard Gong
      struck hard: real horn, trumpets, trombone and tuba now? Do they still sound like
      DUNGEON CRAWLER (the crack on the hit, bright, a little gritty) and not like a stock
      orchestral library? Too harsh, too thin on the attack anywhere?
- [ ] Play each slowly from soft to hard (velocity 20 to 127): the level rises evenly; where
      the tone changes to the next recorded dynamic, nothing jumps or drops.
- [ ] Long held notes (10 s and more) low and high: no click or pulse as the loop turns;
      let go: the recorded release, no click.
- [ ] The extremes: the horn above F5, the tuba below F#1, the trumpets above C6: the note
      hands over to the synthesis smoothly, nothing pitched into chipmunk or growl.
- [ ] In a mix with strings, choir and drums: does the brass cut through without being
      louder than before? In their places: the dry hit first, the tail between the notes.
- [ ] Loud chords, Punch 200 %, Level +12 dB: nothing clips.
- [ ] A project saved with the Session 12 version: the synthesised horns exactly as before.
- [ ] Listen: test-logs `realbrass/` (each instrument `-dry-1-before` / `-dry-2-after`, then
      `-in-its-place-`), and `realbrass-report.txt` for the numbers.

## Session 14: brass and war drums seated

- [ ] The six-part test piece (choir, low strings, brass, melody, war drums, drone) with new
      instances: does the brass sit in the world now (not in front), and does its hit still
      land? Is it still clear, or now too soft or dull? The war drums: in balance?
- [ ] Each brass instrument (Heroic Horn, Brass Fanfare, Doom Brass & Choir, Dwarven Chant with
      Brass up, Hoard Gong struck hard): the attack's first moment dry, a little of the place
      blooming on the held note; warmer and softer at the top; a little wider (still fine in
      mono).
- [ ] Played softly: darker and a little further away; played hard: the crack is there.
- [ ] A project saved with the Session 13 version, Level and Punch untouched: opens a little
      quieter (the new defaults). One with Level or Punch changed: sounds as loud and hits as
      hard as before.
- [ ] Loud chords, Punch 200 %, Level +12 dB: nothing clips.
- [ ] Listen: test-logs `seat/` (each instrument `-dry-1-before` / `-dry-2-after`, then
      `-in-its-place-`; `mix-choir-strings-brass-drums-1-before` / `-2-after`), and
      `seat-report.txt` for the numbers.

## Session 15: Dungeon Synth

- [ ] An existing project: opens with Dungeon Synth at 0 % and sounds exactly as before.
- [ ] New instance, Royal Strings in the Throne Room, a slow dark melody: roll Dungeon Synth
      0 -> 25 -> 50 -> 100 % in Classic. 25 %: a little older and darker; 50 %: clearly a
      90s tape; 100 %: the full cassette sound (muffled top, wobble, a little hiss, nearly
      mono, a big grainy hall). Too much anywhere? Is the hiss ever annoying?
- [ ] The same in Modern: warmer, saturated, the chorus on the strings, the lush dark hall;
      only a hint of lo-fi. Does it still sound like dungeon synth, not just "reverb"?
- [ ] Move the roller and flip the Era while a chord is held: no click, no jump in loudness.
- [ ] The same instrument (Lock instrument) in the Crypt, the Necropolis Gates and the Dwarven
      Hall at 100 %: still three different places?
- [ ] With it on: Effects to 0 % (the stage stays, the place and hall go), the MIX section's
      Reverb down (the hall goes down too), Punch on the brass (still hits), Bypass (the dry
      instrument, untouched).
- [ ] The factory presets in "Dungeon synth" (and searching "dungeon synth", "lo-fi", "retro",
      "cassette"): Cassette Castle Theme, Modern Dungeon Synth Pad, Crypt Organ Drone, Lo-fi
      Fantasy Flute... do they sound like the genre? Which is the best and the worst?
- [ ] DUNGEON CRAWLER FX on a guitar or a synth: the same roller and switch between Travel and
      the switches.
- [ ] Listen: test-logs `dungeonsynth/` (each instrument `-0-off`, `-classic-50`, `-classic-100`,
      `-modern-50`, `-modern-100`; the FX's Crypt and Gates, all five in one file each), and
      `dungeonsynth-report.txt` for the numbers.

## Session 16: Dungeon Synth made unmistakable

The developer heard almost no difference in Session 15 (the place covered the stage). Now:

- [ ] An existing project: opens with Dungeon Synth at 0 % and sounds exactly as before.
- [ ] Royal Strings in the Throne Room, a slow dark melody, Classic: 0 -> 25 -> 50 -> 100 %.
      100 % should be obvious at once even to someone who does not know the genre: a
      wobbling, hissing, crunchy, muffled, nearly mono old tape in a huge cold hall. 50 %
      clearly that; 25 % only a little darker and older. Is the hiss too loud? The grit too much?
- [ ] The same in Modern: warm and wide (the ensemble chorus), saturated, a softer top, a very
      long dark hall, no crunch. Classic and Modern should sound like two different records.
      (Session 18: Modern rebuilt; see below.)
- [ ] Move the roller and flip the Era while a chord is held: no click, no jump in loudness.
- [ ] Lock instrument: the same instrument in the Crypt, the Necropolis Gates and the Dwarven
      Hall at 100 %: still three different places?
- [ ] With it on: Effects to 0 % (the tape stays on the dry instrument, the place and hall go),
      the MIX section's Reverb down (the hall goes down too), Mix to 0, Punch on the brass,
      Bypass (the dry instrument, untouched).
- [ ] The "Dungeon synth" presets (85-100 % now): do they sound like the genre at once?
- [ ] DUNGEON CRAWLER FX on a guitar or a synth: the same.
- [ ] Listen: test-logs `dungeonsynth/`: each instrument's `-ab-0-classic100-modern100.wav`
      first (0 %, Classic 100 %, Modern 100 % back to back), then `-classic-50`, `-modern-50`.

## Session 18: DUNGEON SYNTH Modern, a lush warm vintage sampler

- [ ] Listen: test-logs `dsmodern/`: each instrument's `-ab-0-modern-before-modern-after.wav`
      (0 %, Modern 100 % as in Session 17, Modern 100 % now, back to back), then
      `-modern-100-1-before` against `-modern-100-2-after`, then `-modern-25-after` and
      `-modern-50-after`. Modern at 100 % should sound expensive and nostalgic: warm and
      enveloping, a smooth (not dull) top, no fizz, no grit, no hiss you notice, a wide slow
      ensemble and a long dark hall that blooms; never thin or cheap. 25 / 50 %: subtle, but there.
- [ ] Royal Strings in the Throne Room, the Dark Choir Pad, the Lute: Modern 0 -> 25 -> 50 ->
      100 %: no jump in loudness, no click; then flip to Classic and back: Classic as before.
- [ ] Classic: exactly as in Session 17 (bit for bit in the tests).

## Session 17: real woodwinds

- [ ] Tin Whistle (Village Green), Wooden Flute (Forest Edge), Shepherd's Flute (Foothills),
      Murky Reed (Bog): real flute, clarinet and bassoon now? Does each still sound like its
      place (a small weary folk whistle; two uneasy wooden flutes; a breathy flute far off on
      the hills; a murky low reed) and like DUNGEON CRAWLER, not a stock orchestral library?
- [ ] The attacks: breathy and natural, a soft tongue on harder notes; the key and breath
      noise subtle (Breath / Murk add more).
- [ ] Play each from soft to hard: the level and tone rise evenly, no jump where the recorded
      dynamic changes.
- [ ] The mod wheel (Expression): up, the note swells a little and brightens; on the flutes the
      vibrato comes in. Move it fast while holding a note: no click.
- [ ] Legato: hold a note and play the next before letting go: a slur (no new attack, a short
      glide), no gap, no click. A chord: all its notes sound.
- [ ] Long notes (10 s and more): no pulse as the loop turns; let go: the recorded release.
- [ ] The extremes (the Wooden and Shepherd's Flutes below G3, the whistle very high, the reed
      very low): the note hands over to the synthesis smoothly.
- [ ] DUNGEON SYNTH at 50 and 100 % on each: still works (Classic, Modern).
- [ ] A project saved with the Session 16 version: the synthesised winds exactly as before.
- [ ] Listen: test-logs `realwinds/` (each `-dry-1-before` / `-dry-2-after`, then
      `-in-its-place-`, then `-expression-swell`), `realwinds-report.txt` for the numbers
      (the cents of every set and instrument).

## 1.0.0 release (the installer, Windward Audio)

Install from the customer download **DUNGEON CRAWLER-1.0-macOS** (a signed CI run or the
GitHub Release), on a Mac that had an earlier build installed and a project saved with it:

- [ ] The download holds only `DUNGEON CRAWLER Installer.pkg` and `DUNGEON CRAWLER - Read Me.pdf`.
- [ ] Double-clicking the package opens without a Gatekeeper warning; the window title,
      pages and wording follow the [installer standard](INSTALLER_STANDARD.md) (Windward Audio, support@windwardaudio.com).
- [ ] GarageBand and Logic list **Windward Audio → DUNGEON CRAWLER** and **Windward Audio →
      DUNGEON CRAWLER FX**, each once; nothing is listed under the old company name.
- [ ] Logic → Plug-in Manager shows version 1.0.0 for both.
- [ ] The project saved with the earlier build opens with the same places, faders and sound.
- [ ] Presets saved earlier appear under "Your presets" in both plug-ins; "Show presets
      folder" opens `~/Library/Windward Audio/DUNGEON CRAWLER[ FX]/Presets`; the old folder
      `~/Library/Application Support/DungeonCrawler` is still there.
- [ ] Logic's own presets saved earlier appear in its preset menu for each plug-in.
- [ ] Uninstall as the Read Me says: both plug-ins disappear from the hosts after a restart.

## Warmth pass (build 2 of 1.0.0)

Install the signed `DUNGEON CRAWLER Installer.pkg` from the manual CI run (or the developer
artifact). In GarageBand, on a software instrument track with DUNGEON CRAWLER:

- [ ] A project saved with build 1 opens with the same instrument, place and settings, and now
      sounds warmer: stereo (not mono), a soft hazy top with a little tape hiss while it plays
      (none in silence), slow tape wow, a long dark hall, glued and a little louder.
- [ ] Chords on pads and strings are wide in Modern, narrower but still stereo in Classic; the
      bass stays centred.
- [ ] The Dungeon Synth roller (the Grit) at 50 % and 100 %: thicker and darker, crushed but
      warm; never fizzy, harsh or thin; the level about the same.
- [ ] In the plug-in's generic controls (or automation): Warmth 0 % is the sound of build 1;
      Mono folds to mono and back without a click.
- [ ] Listen to the CI artifact `DUNGEON-CRAWLER-warmth-renders`: each `before/<instrument>-*.wav`
      (build 1) against `after/<instrument>-<era>-grit<0|50|100>.wav` (this build).
