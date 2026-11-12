#pragma once
// Where DUNGEON CRAWLER and DUNGEON CRAWLER FX keep the user's own files (1.0), and the
// one-time copy of the files from the folders used before 1.0.
//
//   macOS: ~/Library/Windward Audio/DUNGEON CRAWLER/Presets      your presets (.dcpreset)
//          ~/Library/Windward Audio/DUNGEON CRAWLER/scenes       drop-in scene art
//          ~/Library/Windward Audio/DUNGEON CRAWLER FX/...       the same for the effect
//   (elsewhere the same below JUCE's user application data folder, e.g. ~/.config)
//
// Before 1.0 both plug-ins shared ~/Library/Application Support/DungeonCrawler/{Presets,scenes},
// and hosts kept their own presets (.aupreset) under ~/Library/Audio/Presets/<old company>/...
// On first launch each plug-in COPIES (never moves) those into its new folders; the host
// presets go to ~/Library/Audio/Presets/Windward Audio/<plug-in name>, where hosts now look.
// The old folders are left as they were (a copy is kept), existing files are never
// overwritten, and a marker file in the new folder makes it happen once.
#include <juce_core/juce_core.h>

namespace dc::userdata {

inline constexpr const char* companyFolder = "Windward Audio";
inline constexpr const char* instrumentFolder = "DUNGEON CRAWLER";
inline constexpr const char* effectFolder = "DUNGEON CRAWLER FX";
inline constexpr const char* markerName = ".migrated-to-1.0";

// The folder JUCE calls the user application data directory (~/Library on macOS).
juce::File library();
// <library>/Windward Audio/DUNGEON CRAWLER[ FX]
juce::File pluginFolder(bool instrument, const juce::File& lib = library());
juce::File presetsFolder(bool instrument, const juce::File& lib = library());
juce::File scenesFolder(bool instrument, const juce::File& lib = library());
// What hosts show for this build (CMakeLists.txt): "Windward Audio" and "1.0.0".
juce::String companyName();
juce::String versionString();
// True when this binary is the instrument (the build's own identity, CMakeLists.txt).
bool buildIsInstrument();

struct Migration {
    bool ran = false;        // there was something old to copy (and no marker yet)
    bool complete = false;   // every copy succeeded (the marker was written)
    int copied = 0;          // files copied
    int kept = 0;            // files that already existed in the new place (left alone)
};
// Copy the pre-1.0 files below `lib` into this plug-in's new folders (see above).
Migration migrate(bool instrument, const juce::File& lib = library());
// migrate() once per process and plug-in kind; never throws, never blocks for long.
void migrateOnce(bool instrument);

} // namespace dc::userdata
