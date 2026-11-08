# DUNGEON CRAWLER

Two dark-fantasy plug-ins for macOS (AU and VST3; GarageBand and Logic Pro) by Windward
Audio, built from this one repository. A painted map of a dark realm under a red moon is their interface.

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

1. Download `WindwardAudio-DungeonCrawler-macOS-Universal` from the latest green
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

Updating from a version made before the company name changed to Windward Audio: the
installer removes the earlier copies, so hosts list each plug-in once, under Windward
Audio. Your projects and presets keep working (the plug-ins' identity codes and parameters
are unchanged). On its first launch each plug-in copies your user presets, drop-in scenes
and host presets from the folders earlier versions used to its own folder,
`~/Library/Windward Audio/DUNGEON CRAWLER/` (the instrument) or
`~/Library/Windward Audio/DUNGEON CRAWLER FX/` (the effect), and the host presets also to
`~/Library/Audio/Presets/Windward Audio/<plug-in>/`, where the host's preset menu finds
them. The originals are kept.

## The sound (Direction Update 2)

Every place now transforms the instrument obviously at the defaults (Intensity 100 %,
Mix 100 %); Intensity 50 % is still clearly audible, 0 % is the dry signal. Spaces are a
main voice: most places use one of 33 generated spaces (huge halls with slow bloom,
reversed swells, caverns with far echoes and coin glints, small warm rooms). Levels stay
matched between places. The places the developer liked are unchanged or changed only as
noted ([docs/DEVLOG.md](docs/DEVLOG.md), Session 6).

## The instrument (DUNGEON CRAWLER)

**GarageBand:** make a Software Instrument track; in its instrument slot (Smart Controls
(B) → Plug-ins, or the Library) choose Audio Units → Windward Audio → DUNGEON CRAWLER.
**Logic Pro:** a Software Instrument track's Instrument slot → AU Instruments → Windward
Audio → DUNGEON CRAWLER. Play your MIDI keyboard or the piano roll.

- The map and the places work as in the effect: **choosing a place chooses its
  instrument and its space**. A note held while you travel keeps sounding (the space
  morphs around it; the next note plays the new place's instrument).
- **The instrument panel** sits under the world (the window is 960 x 756 at 100 %): where
  the instrument belongs and its name, ‹ › to step through the forty and a chooser for any
  of them (choosing one locks it), **Lock instrument**, the pitch-bend range; its five
  macros (Tone, Attack, Release, Body and its own signature control; their names follow
  the instrument: Pluck, Strike, Ring, Glint, Course…), **Vowel** and **Ensemble** (on the
  instruments that sing with the recorded voice: Soft Choir, Monk Chant, Dark Choir Pad,
  Dwarven Chant, Doom Brass & Choir, Music Box; dimmed on the others; 50 % is the
  instrument as designed), **Level**, **Intensity**, the **Effects** roller, the bend and
  mod wheels and the sustain pedal (they follow your keyboard and can be dragged), and
  an on-screen keyboard that lights up with the notes (click it to play).
- **Effects** (a roller like a mod wheel, on the instrument panel and in the effect's
  controls, next to Intensity): how much of the place's effects. 0 % is nearly the dry
  sound, 100 % the place as designed (sessions from before it existed open at 100 %),
  up to 150 % stronger: the reverbs, echoes, voices and character effects go further,
  level-matched. Automatable, saved with presets and sessions; drag it, scroll it, or
  double-click it back to 100 %.
- Playing: velocity (level and tone), sustain pedal, pitch bend (±2 semitones by
  default), the mod wheel (vibrato, brightness, tremulant, swell or growl, as fits the
  instrument; the panel says which), up to 4–12 notes at once depending on the
  instrument, click-free voice stealing.
- **Presets** (toolbar): *Instruments* (all forty, each in its own place with its macros
  set), *Instruments elsewhere* (locked instruments in other places: the Tavern guitar in
  the Boss Lair, church bells in the Crystal Cavern…) and the effect's *Journeys* (the
  instrument changes with each place of the route).
- Intensity 0 % (or Effects 0 %) plays the instrument dry; Bypass too.
- **The window reopens as you left it**: closed and opened again, or with the project,
  it shows the same place (or the map), the same panels open and scrolled where they
  were, the controls shown or hidden, at the same size; presets never move the window.

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
slot → Audio Units → Windward Audio → DUNGEON CRAWLER FX. Double-click the slot to open the
window (if you see plain sliders, switch the window's view menu from "Controls" to
"Editor"). **Logic Pro:** an Audio FX slot → Audio Units → Windward Audio → DUNGEON CRAWLER FX.

1. The window opens on the **map**. Hover a landmark to see its name; click it: a red
   path shows the journey and the view zooms into the place.
2. Inside a location, the **rings** (and the list, top right) are its places: click one
   to travel there. The breadcrumb (top left) leads back: "‹ Map" returns to the map
   without changing the sound; the location's name travels back to its own scene.
3. Automate **Location** and **Sub-location** to move through the world over the song;
   or use a **Journey** (below).

## Mix (both plug-ins)

Several instances in a song can drown in reverb. **Mix** (the toolbar; in the instrument
also on its panel) opens a small panel over the left of the scene, like the Journey
panel. Each slider is 0-150 %; 100 % is the place as designed (double-click returns
there). The Effects roller stays the master over all of them.

| Control | What it does |
|---|---|
| **Reverb / Space** | All the place's reverbs and convolution spaces (0 %: none; 150 %: more, and longer tails). |
| **Echoes** | Delays, tape echoes, slap-backs (and the echo-type signatures: Valley Answer, Canyon Echoes...). |
| **Modulation** | Chorus, shimmer, flutter, the pitch voices and drones, wobbles. |
| **Character** | Drive, filters, resonators, bells, and the place's other signature effects. |
| **Atmosphere** | The Atmosphere layer's level. |
| **Low Cut** | Removes reverb mud below 80-600 Hz (Off by default). |
| **Pre-delay** | Moves the reverbs later (the dry hit first) or earlier (-40 to +120 ms). |
| **Duck** | The reverb tails dip while you play and bloom in the gaps (0 % = off). |

All of them are automatable and saved with presets and sessions, and they move without
clicks. The loudness stays matched: turning a group down keeps the overall level. The low
cut and the duck never move the level matcher, so the dry stays exactly where it was.
Each place's signature effect counts in the group it sounds like. The Advanced drawer
has a new group, **XI. Atmosphere**: on/off, level and density.

**The instrument in a song**: the instruments you mostly play in a mix (brass, strings,
drums, plucks) now send less to their place's reverbs and echoes by default, and their
reverb starts 10-30 ms later, so the dry hit comes first. Reverb / Space at 150 % still
gives the place's full space and more.

**Brass** (Brass Fanfare, Heroic Horn, Doom Brass & Choir, the Dwarven Chant's low brass)
has a quicker, brighter, harder attack, more presence (2-4 kHz) and a stronger velocity
response, at the same sustain level.

**Punch** and **Punch Time** (instrument panel): every note hits louder and settles back to its
sustain level over Punch Time (20-500 ms). 100 % is the instrument's own amount: about
+10 to +18 dB on the brass (Session 11: +9 dB), +2 dB on the War Drums (more would make their recorded velocity layers step), a little on the plucks, none on pads, choirs and
organs. The top of the slider is +12 dB on any instrument (+18 dB on the brass) and the bottom is off. It responds to velocity, it is
click-free, and a safety limiter keeps it from ever clipping.

Projects and user presets saved before this version open exactly as they sounded,
including the instrument's old defaults. Init or any factory preset brings the new sound.

**The horns hit (Session 12)**: Tower Top's Heroic Horn, the Great Hall's Brass Fanfare, the
Final Boss Lair's Doom Brass & Choir and the Dwarven Hall's low brass crack on the hit: a
faster rise, the lips driven harder and an upper-mid edge for the first ~120 ms, a steeper
velocity response, and a soft shaping of the hit just under full scale so that a loud chord
keeps its punch instead of being pulled down by the output's safety limiter. Their own
Punch is now +12 dB on the horn (the Doom Brass +18 dB, the fanfare +10 dB, the Dwarven
Chant +7 dB), falling away steeply after the crack, and Punch at the top reaches +18 dB on
the brass (+21 on the Doom Brass). In their places their reverb is drier, starts later and dips
under each hit, so the dry attack lands first. The held note is exactly as loud as before.

**Punch** and **Punch Time** read and work as in LIGHT SEEKER: Punch shows the hit in dB for
the instrument playing (Off at none), Punch Time in ms; type a value into either, or
double-click for the default.

Projects saved with the Session 11 version keep that sound and their Punch settings exactly;
new instances, Init and the factory presets play the Session 12 horns.

**Real brass (Session 13)**: the brass plays recorded horn, trumpet, tenor trombone and tuba
(sustains and staccatos, every dynamic the recordings have), made the instrument's own:
- **Heroic Horn** (Tower Top): two horns, the second another recording a few cents up and a
  moment late (Bell Up: how much of it); the synthesised horns stay under them for weight.
- **Brass Fanfare** (Great Hall): two trumpets an octave above the key and the trombone on it
  (Section).
- **Doom Brass & Choir** (Final Boss Lair): horn and trombone on the note, the tuba an octave
  below, the synthesised octave below for weight, then the choir stab and the doom piano.
- **Dwarven Chant** (Dwarven Hall): its low brass a tuba and a trombone (Brass).
- **Hoard Gong** (Dragon's Hoard): played hard, a trombone and a tuba growl out of the gong.

Every note is the staccato's hit with the sustain under it (dipped under the hit, then
looped, with its own recorded release), levelled on a smooth velocity curve; then the
section is processed as DUNGEON CRAWLER brass: the hit's low end thinned, the lips pushed
(a saturation on the attack, up to the note's own peak), the upper-mid presence band, an
exciter that adds the 2-4 kHz brightness a horn played hard has, and on top the brass's
Punch, presence, attack edge, hit shaping and its place, as in Session 12. The hit is
harder and brighter; the whole is no louder (the melody 0.5 dB under Session 12's, held
notes a little lower). Keys beyond the recordings hand over to the synthesis.

The recordings are **VSCO 2 Community Edition** (Versilian Studios; CC0 1.0, public domain:
[docs/LICENSES/VSCO-2-CE.txt](docs/LICENSES/VSCO-2-CE.txt)). They are not in this repository:
CI fetches them from the official source at a pinned commit (`scripts/ci-fetch-brass.sh`,
cached), checks the licence, and `tools/sample-prep/brass.py` prepares them into the
instrument's bundle (with the licence). A build without them plays the Session 12 synthesis,
sample for sample. Projects saved before this version keep their sound (sound 12: the
synthesised horns); new instances, Init and the factory presets play the recorded brass.

**Brass and war drums seated (Session 14)**: the brass sits in the world with the other
instruments instead of in front of them, its hit still landing. Every brass instrument (the
Tower Top horn, the Great Hall fanfare, the Final Boss Lair's doom brass, the Dwarven Hall's
low brass, the Hoard Gong's growl) is about 2.5 dB lower by default (the chant and the gong:
their brass layer) with a little less Punch (horn +10.5 dB, fanfare +9, doom brass +16, the
chant +6; the same fall after the crack), and gets the same baseline treatment as the
others: a touch more of its place's reverb and echoes on the held note (the reverb still
starts 50 ms after the hit and dips under it), gentle tape warmth (the hit rounded, the held
note untouched), a soft top (a gentle high shelf above about 7 kHz, deeper and lower played
softly: darker and further away when quiet) and a little ensemble width (mono-safe). The
**War Drums** (Mountain Pass) are about 2.5 dB lower too (Punch +1.5 dB). Everything else
plays exactly as before. Projects saved with the Session 13 version open with this sound:
where Level and Punch were left at their defaults they get the new defaults; where they were
set, they are kept as they were heard (the Level raised by what the new default takes away,
Punch at the same dB on the hit). Older projects (sound 12 and before) are unchanged.

**Real woodwinds (Session 17)**: the woodwinds play recorded flute, piccolo, oboe, clarinet
and bassoon from the same CC0 library as the brass (sustains looped with their own recorded
release, vibrato sustains where the library has them, staccatos as the tongued attack, every
dynamic as a velocity layer), each made into DUNGEON CRAWLER's own dark, weary wind:
- **Tin Whistle** (Village Green): the flute (the piccolo above E6) made small and folky: its
  low end cut, a woody body, a darker top, its pitch a little tired and wandering, a breathy
  tongued attack (Breath: how much air).
- **Wooden Flute** (Forest Edge): two flutes not quite agreeing (the second another recording,
  flat by Detune, a moment late, the two wandering apart), darker, woody, no vibrato at rest:
  uneasy.
- **Shepherd's Flute** (Foothills): the flute far off on the hills: very breathy, soft-edged,
  open and distant, its vibrato takes at rest, more of the place's echoes and space.
- **Murky Reed** (Bog): the clarinet in its lowest register, the bassoon below it, a darkened
  oboe faintly on the unison from B-flat 3; a dark, closed top (Murk closes it further), a
  low woody body, the breath in the mud.

The **mod wheel** is their **Expression**: a little louder and brighter as it rises (on the
flutes into their vibrato takes). **Overlapping notes slur** (legato): a note played while the
previous one is still held enters without a new tongued attack, gliding from it, and the
previous one lets go; notes struck together (a chord) are not slurred. Keys beyond the
recordings hand over to the synthesis. A build without the samples, and any project saved
before this version (sound 14), plays the synthesised winds exactly as before; new
instances, Init and the factory presets play the recorded woodwinds (sound 15). The folk
winds (Tin Whistle, Wooden Flute, Shepherd's Flute) are ready for the developer's own alto
recorder and ocarina: prepared from the Samples repository as `recorder-alto-sus` (and
optionally `-vib`, `-stac`) or `ocarina-sus`, they are played in place of the flute.

## Dungeon Synth (both plug-ins, Session 15; made unmistakable in Session 16)

A **DUNGEON SYNTH** roller with an **Era** switch makes any instrument (or anything through
the FX) sound like dungeon synth instead of general dark fantasy. On the instrument panel it
sits to the right of the keyboard; in the FX between Travel and the switches. **0 %** (the
default, and what every project saved before it opens at) is the sound exactly as before;
**25 %** subtle, **50 %** clearly dungeon synth, **100 %** the full genre sound. Drag it, use
the mouse wheel, double-click for 0 %; both controls are automatable and saved with presets
and projects.

- **Classic** (the 90s cassette demo: an old tape of a cheap keyboard in a huge cold
  castle): a cheap sampler (down to about 13 kHz with its aliasing grit, about 8 bits), the
  keyboard's light chorus, a big dark, grainy old-digital hall at a high wet level, cassette
  wow (about 0.5 %, slow and drifting) and flutter, tape hiss riding under the music (about
  -45 dBFS on bright material, gone in silence), a thin bottom, a boxy, honky middle, a dark
  top (5.5 kHz), a nearly mono image.
- **Modern** (lush, warm, nostalgic: a warm vintage sampler, not a broken one; Session 18):
  gentle bit and rate reduction (12 bits and 32 kHz at 100 %, 29.4 kHz at 44.1 kHz) between
  clean linear-phase filters (smooth, rounded, no fizz or noise), warm, gentle tape saturation
  on the body (below about 2.5 kHz, the top left clean), a fuller low-mid and a smooth (not
  muffled) top, a deep, slow, wide vintage ensemble chorus (strongest on pads, strings, choirs
  and organs, almost none on drums), a long, dark modulated hall that blooms, a hint of slow
  wow and very little hiss.

It works on the whole sound, after the place (as a cassette records everything): the place's
space and effects come first, so the Crypt, the Necropolis and the Dwarven Hall still sound
different, and then all of it goes through the tape (only the hiss goes in before the place,
which shapes it as it shapes the music; the hall takes on the place's length and tone). Its hall follows **Intensity**,
**Effects** (towards 0 %: the stage stays on the dry instrument, the hall goes with the
place), the **MIX** section's Reverb and **Mix**. **Lock instrument** and **Punch** work as
before; **Bypass** is always the untouched input. Loudness stays within about 1.5 dB as it
moves (its own level matcher), and moving it or switching the Era is click-free.

Factory presets built around it (category **Dungeon synth**, at the end of the preset list;
search "dungeon synth", "lo-fi", "retro" or "cassette"), at 85-100 %: Cassette Castle Theme,
Modern Dungeon Synth Pad, Crypt Organ Drone, Lo-fi Fantasy Flute, Tape Harp of the Grove,
Frozen Tower on Cassette, Hall of the Old Kings, Starlit Modern Pad, Necropolis Choir Tape,
Harpsichord Demo Tape; in the FX: Cassette Dungeon, Lo-fi Crypt, Modern Dark Hall.

## Search (both plug-ins)

The magnifier at the top right of the window (or **Cmd+F**) opens the search over the
scene. Type a name, a sound or a mood: results appear as you type (any case, partial
words, typos: "crpyt" finds the Crypt, "dwarvn" the Dwarven Hall and Dwarven Chant).
They are grouped as **Instruments**, **Places** and **Presets**, each with a small icon,
where it belongs and its tags.

- **What it searches**: the ten locations and all 40 places, the 40 instruments (name,
  family, location; the instrument only), every factory preset and journey, and their
  tags: sound and mood ("choir", "chant", "war drums", "organ", "bells", "pad", "drone",
  "echo", "dark", "sinister", "epic", "eerie", "sacred", "watery", "huge", "intimate"...)
  and the recordings they use ("guitar", "banjo", "djembe", "piano", "violin",
  "kalimba", "voice", "whispers").
- **Choosing**: arrow keys and Enter, or a click. It does what choosing it by hand does:
  a place travels there (the path on the map and the zoom, or the morph from scene to
  scene), an instrument is loaded as the panel's chooser loads it (played here, locked),
  a preset loads as from the preset menu. Held notes ring on, Lock instrument is kept
  when you travel, and the window reopens where the search took you.
- **Hover** (or arrow to) a result to read its description. Instruments have a **Hold to
  hear** button: a short phrase while you hold it (never while the host plays).
- **Chips** under the field: Instruments, Places, Presets, and the moods Dark, Epic,
  Eerie, Sacred (click again to clear). With nothing typed, **Recent** lists your last
  eight searches and results (saved with the project).
- **Esc** or a click outside closes it; the click still reaches the map.
- The FX searches places, journeys and presets (it has no instruments).

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
| **Mix** | The MIX section: reverb, echoes, modulation, character, atmosphere, and the reverbs' low cut, pre-delay and duck (see Mix above). |
| **Dungeon Synth** (roller) + **Classic / Modern** | How much it sounds like dungeon synth (0 %: as it is, 100 %: the full genre sound) and which era (see Dungeon Synth above). |
| **Magnifier** (top right), Cmd+F | Search places, instruments, presets and sounds (see Search above). |
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
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen-realbrass build/realbrass       # every brass instrument before / after the recorded brass, dry and in its place
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen-seat build/seat               # the brass and the war drums before / after Session 14, alone and in a short mix
build/DungeonCrawlerTests_artefacts/Release/DungeonCrawlerTests --listen-realwinds build/realwinds     # every woodwind before / after the recorded woodwinds, dry and in its place, an Expression swell
```

The recorded brass and woodwinds (Sessions 13 and 17, CC0) for a local build:

```bash
bash scripts/ci-fetch-brass.sh build/vsco-2-ce build/samples   # fetches the library (pinned), checks its licence, prepares brass and woodwinds
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
| `Source/DSP/` | The modules: from the developer's earlier plug-in (Abyss, Ancient Blood, Choir, Wraith, Echoes, Crypt) and new (Relics, Signature, Atmosphere, Vault + Convolver, Ensemble) |
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
