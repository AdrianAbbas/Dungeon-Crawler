#include "UserData.h"

#include <atomic>

namespace dc::userdata {
namespace {

// The pre-1.0 places. The old company folder is only a path read here, never shown.
constexpr const char* oldAppFolder = "DungeonCrawler";
constexpr const char* oldHostPresetCompany = "Veil Audio";

juce::File oldAppData(const juce::File& lib) {
   #if JUCE_MAC
    return lib.getChildFile("Application Support").getChildFile(oldAppFolder);
   #else
    return lib.getChildFile(oldAppFolder);
   #endif
}

// Which plug-in an .aupreset belongs to: its AU subtype, Dgci (instrument) or Dgcr (effect),
// as an XML plist integer or as a binary plist's big-endian 32-bit integer (marker 0x12).
// -1 when the file does not say.
int aupresetKind(const juce::File& file) {
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() > 4 * 1024 * 1024) return -1;
    const auto text = juce::String::createStringFromData(data.getData(), static_cast<int>(data.getSize()));
    const auto subtypeIs = [&text](const char* number) {
        const int at = text.indexOf("<key>subtype</key>");
        return at >= 0 && text.substring(at, at + 80).contains(juce::String("<integer>") + number + "</integer>");
    };
    if (subtypeIs("1147626345")) return 1;   // 'Dgci'
    if (subtypeIs("1147626354")) return 0;   // 'Dgcr'
    const auto* bytes = static_cast<const char*>(data.getData());
    for (std::size_t i = 0; i + 5 <= data.getSize(); ++i)
        if (bytes[i] == '\x12' && bytes[i + 1] == 'D' && bytes[i + 2] == 'g' && bytes[i + 3] == 'c') {
            if (bytes[i + 4] == 'i') return 1;
            if (bytes[i + 4] == 'r') return 0;
        }
    return -1;
}

struct Copier {
    Migration& m;
    // Copy one file, keeping what is already there; false only on a failed copy.
    bool file(const juce::File& from, const juce::File& to) {
        if (to.exists()) { ++m.kept; return true; }
        if (!to.getParentDirectory().createDirectory()) return false;
        if (!from.copyFileTo(to)) return false;
        ++m.copied;
        return true;
    }
    bool tree(const juce::File& from, const juce::File& to) {
        bool ok = true;
        for (const auto& f : from.findChildFiles(juce::File::findFiles, true, "*"))
            if (!f.getFileName().startsWithChar('.')) ok = file(f, to.getChildFile(f.getRelativePathFrom(from))) && ok;
        return ok;
    }
};

} // namespace

juce::File library() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory); }

juce::File pluginFolder(bool instrument, const juce::File& lib) {
    return lib.getChildFile(companyFolder).getChildFile(instrument ? instrumentFolder : effectFolder);
}
juce::File presetsFolder(bool instrument, const juce::File& lib) { return pluginFolder(instrument, lib).getChildFile("Presets"); }
juce::File scenesFolder(bool instrument, const juce::File& lib) { return pluginFolder(instrument, lib).getChildFile("scenes"); }

juce::String companyName() {
   #ifdef JucePlugin_Manufacturer
    return JucePlugin_Manufacturer;
   #else
    return {};
   #endif
}
juce::String versionString() {
   #ifdef JucePlugin_VersionString
    return JucePlugin_VersionString;
   #else
    return {};
   #endif
}

bool buildIsInstrument() {
   #if defined(JucePlugin_IsSynth) && JucePlugin_IsSynth
    return true;
   #else
    return false;
   #endif
}

Migration migrate(bool instrument, const juce::File& lib) {
    Migration m;
    const auto target = pluginFolder(instrument, lib);
    const auto marker = target.getChildFile(markerName);
    if (marker.existsAsFile()) return m;

    // 1. Presets and drop-in scenes: before 1.0 both plug-ins shared one folder, so each
    //    gets a copy of all of it (as before, both list the same presets).
    const auto old = oldAppData(lib);
    const auto oldPresets = old.getChildFile("Presets"), oldScenes = old.getChildFile("scenes");
    // 2. Host presets (.aupreset), found by the folder the host made and, where the file says,
    //    by the plug-in's AU subtype (before the instrument existed, "DUNGEON CRAWLER" was the effect).
    const auto oldHost = lib.getChildFile("Audio").getChildFile("Presets").getChildFile(oldHostPresetCompany);
    const auto newHost = lib.getChildFile("Audio").getChildFile("Presets").getChildFile(companyFolder)
                            .getChildFile(instrument ? instrumentFolder : effectFolder);
    const char* hostFolders[] = { "DUNGEON CRAWLER", "DungeonCrawler", "DUNGEON CRAWLER FX", "DungeonCrawlerFX" };

    const bool anything = oldPresets.isDirectory() || oldScenes.isDirectory() || oldHost.isDirectory();
    if (!anything) return m;
    m.ran = true;
    Copier copy { m };
    bool ok = true;
    if (oldPresets.isDirectory()) ok = copy.tree(oldPresets, presetsFolder(instrument, lib)) && ok;
    if (oldScenes.isDirectory()) ok = copy.tree(oldScenes, scenesFolder(instrument, lib)) && ok;
    for (const char* name : hostFolders) {
        const auto folder = oldHost.getChildFile(name);
        if (!folder.isDirectory()) continue;
        const bool fxFolder = juce::String(name).endsWithIgnoreCase("FX");
        for (const auto& f : folder.findChildFiles(juce::File::findFiles, true, "*.aupreset")) {
            int kind = aupresetKind(f);
            if (kind < 0) kind = fxFolder ? 0 : 1;
            if ((kind == 1) != instrument) continue;
            ok = copy.file(f, newHost.getChildFile(f.getRelativePathFrom(folder))) && ok;
        }
    }
    if (ok && target.createDirectory() && marker.replaceWithText("Copied from the folders used before 1.0; the originals were kept.\n"))
        m.complete = true;
    return m;
}

void migrateOnce(bool instrument) {
    static std::atomic<bool> done[2] { false, false };
    if (done[instrument ? 1 : 0].exchange(true)) return;
    migrate(instrument);
}

} // namespace dc::userdata
