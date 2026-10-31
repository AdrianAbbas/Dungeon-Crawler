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
