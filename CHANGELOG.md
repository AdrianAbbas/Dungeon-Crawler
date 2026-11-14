# Changelog

All notable changes to DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX (the effect).
1.0 is frozen: only bug fixes go into 1.0.x; new ideas go to [docs/ROADMAP.md](docs/ROADMAP.md).
The warmth pass (build 2) unfroze the sound once before release, at the developer's request; the
hiss fix (build 3) corrects it.

## 1.0.0, build 3 (2026-11-14): the hiss fix

Still version 1.0.0 (not sold yet); the build number (CFBundleVersion) is 3. The Advanced panel
shows it in its header ("1.0.0 (3)").

- **Fixed: a constant hiss.** Build 2's tape-hiss bed followed the output, after the warm hall and
  the leveller, and filled 8-20 kHz up to 29-33 dB under the whole: it never stopped while a tail
  rang, and on dark instruments it was mostly noise. The hiss now follows the notes and the dry
  signal, sits about 46 dB (Classic) / 52 dB (Modern) under the whole at the default, stops within
  400 ms of the last note (even while the hall rings; exactly zero with no note and no input), and
  dips about 10 dB under notes held longer than 2 s.
- **Air** (new parameter, 15 %): the hiss's amount, from none (0 %) to obvious (100 %).
- The top's air now comes from the music: the warm chain's 6 kHz shelf is lifted (it was cut), and
  the Grit's harmonics (the music's own 1.5-5 kHz band, rectified) carry the top as it rises.
- The interface: the DUNGEON SYNTH roller is titled **GRIT** (its parameter is unchanged), with
  **WARMTH** and **AIR** rollers and a small **MONO** switch beside it, in both plug-ins.
- Projects and presets saved before load unchanged (the Audio Unit codes, the VST3 class IDs and
  every parameter ID are the same); they open with Air at 15 %.

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
