# Changelog

All notable changes to DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX (the effect).
1.0 is frozen: only bug fixes go into 1.0.x; new ideas go to [docs/ROADMAP.md](docs/ROADMAP.md).
The warmth pass (build 2) unfroze the sound once before release, at the developer's request.

## 1.0.0, build 2 (2026-11-13): the warmth pass

Still version 1.0.0 (not sold yet); the build number (CFBundleVersion) is 2.

- **Warmth** (new parameter; on, 100 %, on the instrument; off on the FX): a warm character chain
  on every instrument and both Eras: a decorrelated ensemble and per-voice detune, tape wow and
  flutter, a sloped (not cut) top with an air of soft tape hiss that follows the music, gentle tape
  saturation and a slow bus compressor, a long dark stereo hall, 40-80 Hz warmth and a broad
  200-500 Hz cut, the bass mono, the image held stereo (never mono). **Mono** (new, off): a
  deliberate fold to mono.
- The **Dungeon Synth** roller is now the **Grit**: oversampled asymmetric tube/tape saturation, a
  gentle crush (12 bits or more), a soft grain and a dark tilt, level-matched. Classic no longer
  folds the image to mono or cuts the top like a wall; Modern no longer pushes the image out of
  phase.
- Projects and presets saved before load unchanged (the Audio Unit codes, the VST3 class IDs and
  every parameter ID are the same); they open with Warmth on.

## 1.0.0 (2026-11-12)

First release, by **Windward Audio**. No change to the sound or the interface.

- Hosts list the plug-ins as "Windward Audio: DUNGEON CRAWLER" and "Windward Audio:
  DUNGEON CRAWLER FX", version 1.0.0. Bundle IDs `com.windwardaudio.dungeoncrawler` and
  `com.windwardaudio.dungeoncrawlerfx`.
- Projects and presets saved with earlier builds load unchanged (the Audio Unit codes, the
  VST3 class IDs and every parameter ID are the same).
- Your presets and scenes now live in `~/Library/Windward Audio/DUNGEON CRAWLER/` and
  `~/Library/Windward Audio/DUNGEON CRAWLER FX/`; host presets in
  `~/Library/Audio/Presets/Windward Audio/`. On first launch they are copied from the
  folders used before (the originals are kept).
- Signed with Developer ID, notarised by Apple, and installed by a signed installer,
  `DUNGEON CRAWLER Installer.pkg` (Audio Unit and VST3, for all users), which silently removes copies
  installed before 1.0. The download holds only the installer and a Read Me (PDF).
