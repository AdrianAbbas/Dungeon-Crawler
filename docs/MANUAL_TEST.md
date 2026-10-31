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
