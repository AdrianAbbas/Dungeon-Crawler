# Changelog

All notable changes to DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX (the effect).
1.0 is frozen: only bug fixes go into 1.0.x; new ideas go to [docs/ROADMAP.md](docs/ROADMAP.md).

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
  `DUNGEON CRAWLER.pkg` (Audio Unit and VST3, for all users), which silently removes copies
  installed before 1.0. The download holds only the installer and a Read Me (PDF).
