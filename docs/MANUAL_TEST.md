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
   Audio Units → Veil Audio → DUNGEON CRAWLER.
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

## M7 + M8: the engraved map and the scenes

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
- [ ] Optional: put your own PNG in `~/Library/Application Support/DungeonCrawler/scenes/`
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
