# Art sources

Every image in DUNGEON CRAWLER is **original and generated in code**: no photographs,
no third-party illustrations, nothing copied from the mood reference (which is mood
only and is not in this repository).

## How the art is made

- **Engraving renderer** (`Source/UI/Engraving.*`): a scene is drawn as *fields* (a
  tone from black to bare paper, a hatch direction, a blood-red accent) plus ink
  outlines. The renderer turns the fields into an engraving: parallel hatching whose
  line width follows the tone, cross-hatching in the shadows, slightly wavering lines,
  a dark parchment with fibres, foxing and a vignette, and warm glows for flames.
- **Scenes** (`Source/UI/Scenes.cpp`: the map; `Source/UI/ScenePlaces.cpp`: the 40
  places) are compositions of procedural motifs (`Source/UI/Motifs.h`: mountains,
  trees, towers, arches, columns, torches, candles, masonry...). Text uses the system
  serif ("Georgia" where available).
- Rendering happens on a background thread (`Source/UI/SceneCache.*`) at the
  window's pixel size (Retina: up to 2×), so the art is always sharp and the message
  thread never waits for it.

## Public-domain engravings

None are used. The brief allows documented public-domain 19th-century engravings
(e.g. Gustave Doré on Wikimedia Commons with a PD licence tag); the procedural
renderer made them unnecessary and avoids any licensing question. If one is added
later, list it here with its source URL, author, date and licence.

| Image | Source | Author / date | Licence |
|---|---|---|---|
| (none) | | | |

## Replacing a scene with your own art (drop-in folder)

Put a PNG (any size; it is scaled to cover the window, 960 × 560 aspect, i.e. about
12:7) at

    ~/Library/Application Support/DungeonCrawler/scenes/<location>/<place>.png
    ~/Library/Application Support/DungeonCrawler/scenes/map.png

Names are lower case with spaces as hyphens and without apostrophes, e.g.
`scenes/dungeon/treasure-room.png`, `scenes/wizards-tower/observatory.png`,
`scenes/castle/courtyard.png` (a location's own scene uses its scene name: Corridors,
Courtyard, Cloister, Forest Edge, Cave Mouth, Village Green, Mire, Foothills, Gates,
Stairwell). Reopen the plug-in window to see it. The map's landmarks and the scenes'
hotspots stay where the procedural art has them.
