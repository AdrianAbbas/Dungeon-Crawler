#pragma once
// Where the two plug-ins keep the user's own files (the company rename): user presets,
// drop-in scenes and a copy of the host's presets, one folder per plug-in:
//   ~/Library/Windward Audio/DUNGEON CRAWLER/      the instrument
//   ~/Library/Windward Audio/DUNGEON CRAWLER FX/   the effect
// (macOS; elsewhere under the user's application data folder, e.g. ~/.config on Linux).
// On the first launch of each plug-in after the rename, everything in the folders older
// versions used is copied there (never moved: the old files stay as they were):
//   ~/Library/Application Support/DungeonCrawler/Presets  -> Presets  (both plug-ins
//   ~/Library/Application Support/DungeonCrawler/scenes   -> scenes    shared these)
//   ~/Library/Audio/Presets/Veil Audio/<plug-in>/*.aupreset (the host's presets, saved
//       under the old company name)  -> Host Presets, and into the host's folder for the
//       new name (~/Library/Audio/Presets/Windward Audio/<plug-in>) so the host's preset
//       menu still lists them. Each goes to the plug-in its Audio Unit type names (aumu:
//       the instrument, aufx: the effect); one whose type cannot be read goes by its folder.
// A file that already exists at the destination is never overwritten. A marker file
// (.migrated) records that the copy ran.
#include <juce_core/juce_core.h>
#include <mutex>

namespace dc::userdata {
inline constexpr const char* company = "Windward Audio";
// The company name before the rename: only to find the old folders.
inline constexpr const char* oldCompany = "Veil Audio";
inline const char* product(bool instrument) { return instrument ? "DUNGEON CRAWLER" : "DUNGEON CRAWLER FX"; }

// The folder the paths start from: ~/Library on macOS (JUCE's
// userApplicationDataDirectory); the tests use a temporary folder.
inline juce::File systemRoot() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory); }

inline juce::File folder(bool instrument, const juce::File& root = systemRoot()) { return root.getChildFile(company).getChildFile(product(instrument)); }
inline juce::File presets(bool instrument, const juce::File& root = systemRoot()) { return folder(instrument, root).getChildFile("Presets"); }
inline juce::File scenes(bool instrument, const juce::File& root = systemRoot()) { return folder(instrument, root).getChildFile("scenes"); }
inline juce::File hostPresets(bool instrument, const juce::File& root = systemRoot()) { return folder(instrument, root).getChildFile("Host Presets"); }
inline juce::File marker(bool instrument, const juce::File& root = systemRoot()) { return folder(instrument, root).getChildFile(".migrated"); }

// The folder older versions used (shared by both plug-ins).
inline juce::File oldFolder(const juce::File& root) {
   #if JUCE_MAC
    return root.getChildFile("Application Support").getChildFile("DungeonCrawler");
   #else
    return root.getChildFile("DungeonCrawler");
   #endif
}

// Copies `source` to `target` unless something is there already.
inline bool copyNew(const juce::File& source, const juce::File& target) {
    return !target.exists() && target.getParentDirectory().createDirectory() && source.copyFileTo(target);
}

// Copies every file under `from` to the same place under `to`, keeping files already
// there. Returns how many it copied.
inline int copyTree(const juce::File& from, const juce::File& to) {
    if (!from.isDirectory()) return 0;
    int copied = 0;
    for (const auto& entry : juce::RangedDirectoryIterator(from, true, "*", juce::File::findFiles))
        if (copyNew(entry.getFile(), to.getChildFile(entry.getFile().getRelativePathFrom(from)))) ++copied;
    return copied;
}

// The Audio Unit type an .aupreset (an XML property list) was saved by: 1 the instrument
// (aumu), 0 the effect (aufx), -1 unknown.
inline int presetKind(const juce::File& file) {
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return -1;
    const auto* dict = xml->getChildByName("dict");
    if (dict == nullptr) return -1;
    for (const auto* e = dict->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->hasTagName("key") && e->getAllSubText() == "type" && e->getNextElement() != nullptr) {
            const auto type = static_cast<juce::uint32>(e->getNextElement()->getAllSubText().trim().getLargeIntValue());
            if (type == 0x61756d75u) return 1;   // 'aumu'
            if (type == 0x61756678u) return 0;   // 'aufx'
            return -1;
        }
    return -1;
}

struct Migration {
    bool ran = false;   // false: it had run before (the marker was there)
    int copied = 0;     // files copied into this plug-in's folder
};

// The one-time copy for one plug-in (see the top of this file).
inline Migration migrate(bool instrument, const juce::File& root = systemRoot()) {
    Migration m;
    if (marker(instrument, root).existsAsFile()) return m;
    if (!folder(instrument, root).createDirectory()) return m;
    m.ran = true;
    m.copied += copyTree(oldFolder(root).getChildFile("Presets"), presets(instrument, root));
    m.copied += copyTree(oldFolder(root).getChildFile("scenes"), scenes(instrument, root));
    // The host's presets under the old company name, in the folders hosts made for each
    // name the plug-ins had (the effect was once listed as DUNGEON CRAWLER / DungeonCrawler).
    const auto hostRoot = root.getChildFile("Audio").getChildFile("Presets");
    const auto newHost = hostRoot.getChildFile(company).getChildFile(product(instrument));
    struct Old { const char* name; int kind; };   // kind: the plug-in a preset there is for when its type cannot be read
    for (const Old old : { Old { "DUNGEON CRAWLER", 1 }, Old { "DUNGEON CRAWLER FX", 0 }, Old { "DungeonCrawler", 0 }, Old { "DungeonCrawlerFX", 0 } }) {
        const auto from = hostRoot.getChildFile(oldCompany).getChildFile(old.name);
        if (!from.isDirectory()) continue;
        for (const auto& entry : juce::RangedDirectoryIterator(from, true, "*", juce::File::findFiles)) {
            const auto& file = entry.getFile();
            int kind = file.hasFileExtension("aupreset") ? presetKind(file) : -1;
            if (kind < 0) kind = old.kind;
            if (kind != (instrument ? 1 : 0)) continue;
            const auto relative = file.getRelativePathFrom(from);
            if (copyNew(file, hostPresets(instrument, root).getChildFile(relative))) ++m.copied;
            copyNew(file, newHost.getChildFile(relative));
        }
    }
    marker(instrument, root).replaceWithText("Copied " + juce::String(m.copied) + " files from the folders of older versions on "
                                             + juce::Time::getCurrentTime().toISO8601(true) + ". The originals were kept.\n");
    return m;
}

// Once per process and plug-in (its first instance).
inline void migrateOnce(bool instrument) {
    static std::once_flag instrumentOnce, effectOnce;
    std::call_once(instrument ? instrumentOnce : effectOnce, [instrument] { migrate(instrument); });
}
}   // namespace dc::userdata
