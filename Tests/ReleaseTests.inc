// 1.0.0 release: the company and version hosts show, the user's files copied from the
// folders used before 1.0 (presets, drop-in scenes, host presets), and sessions saved before
// 1.0 (Tests/fixtures) loading unchanged in both plug-ins. Included by DungeonCrawlerTests.cpp.
#include "UserData.h"

namespace {
// The PARAM id/value pairs of a saved state.
std::vector<std::pair<juce::String, double>> fixtureParams(const juce::XmlElement& xml) {
    std::vector<std::pair<juce::String, double>> out;
    for (auto* child : xml.getChildWithTagNameIterator("PARAM"))
        out.push_back({ child->getStringAttribute("id"), child->getDoubleAttribute("value") });
    return out;
}

void checkOldSession(Suite& s, PluginKind kind, const char* file) {
    const auto label = juce::String(kind == PluginKind::Instrument ? "instrument" : "effect");
    const auto path = juce::File(DC_SOURCE_DIR).getChildFile("Tests/fixtures").getChildFile(file);
    auto xml = juce::XmlDocument::parse(path);
    if (!s.check(xml != nullptr, ("old session fixture parses (" + label + ")").toStdString(), path.getFullPathName().toStdString())) return;
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary(*xml, data);
    DungeonCrawlerProcessor p(kind);
    p.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    int matched = 0, mismatched = 0;
    std::string first;
    for (const auto& [id, value] : fixtureParams(*xml)) {
        auto* raw = p.parameters.getRawParameterValue(id);
        if (raw == nullptr) { if (mismatched++ == 0) first = id.toStdString() + " is not a parameter"; continue; }
        if (std::abs(raw->load() - value) <= 1.0e-5 * std::max(1.0, std::abs(value))) ++matched;
        else if (mismatched++ == 0) first = id.toStdString() + ": " + fmt(raw->load(), 4) + " vs " + fmt(value, 4);
    }
    s.check(mismatched == 0 && matched >= 6, ("a session saved before 1.0 loads every parameter unchanged (" + label + ")").toStdString(),
            std::to_string(matched) + " matched, " + std::to_string(mismatched) + " not " + first);
    s.check(p.presetName == xml->getStringAttribute("presetName"), ("its preset name is kept (" + label + ")").toStdString(), p.presetName.toStdString());
    // Saved again with 1.0 and loaded into a fresh instance: the same values.
    juce::MemoryBlock again;
    p.getStateInformation(again);
    DungeonCrawlerProcessor q(kind);
    q.setStateInformation(again.getData(), static_cast<int>(again.getSize()));
    int differ = 0;
    for (auto* param : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            if (auto* other = q.parameters.getParameter(ranged->getParameterID()))
                if (std::abs(other->getValue() - ranged->getValue()) > 1.0e-6f) ++differ;
    s.check(differ == 0, ("re-saved with 1.0 it round-trips (" + label + ")").toStdString(), std::to_string(differ) + " differ");
}

juce::File oldAppDataIn(const juce::File& lib) {
   #if JUCE_MAC
    return lib.getChildFile("Application Support").getChildFile("DungeonCrawler");
   #else
    return lib.getChildFile("DungeonCrawler");
   #endif
}

juce::String aupreset(const char* subtype) {
    return juce::String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\"><dict>"
                        "<key>manufacturer</key><integer>1449485676</integer><key>name</key><string>x</string>"
                        "<key>subtype</key><integer>") + subtype + "</integer><key>type</key><integer>1635083896</integer>"
                        "<key>version</key><integer>0</integer></dict></plist>\n";
}
} // namespace

void testRelease(Suite& s) {
    s.section("release 1.0.0 (company, version, user data, old sessions)");
    s.check(dc::userdata::companyName() == "Windward Audio", "hosts show the company Windward Audio", dc::userdata::companyName().toStdString());
    s.check(dc::userdata::versionString() == "1.0.0", "the version is 1.0.0", dc::userdata::versionString().toStdString());
    s.check(dc::userdata::pluginFolder(true, juce::File("/L")).getFullPathName() == "/L/Windward Audio/DUNGEON CRAWLER"
         && dc::userdata::presetsFolder(false, juce::File("/L")).getFullPathName() == "/L/Windward Audio/DUNGEON CRAWLER FX/Presets",
            "user data lives in Windward Audio/DUNGEON CRAWLER and DUNGEON CRAWLER FX");
    {
        DungeonCrawlerProcessor fx(PluginKind::Effect), inst(PluginKind::Instrument);
        s.check(fx.userPresetFolder() == dc::userdata::presetsFolder(false) && inst.userPresetFolder() == dc::userdata::presetsFolder(true),
                "each plug-in saves its presets in its own Windward Audio folder", inst.userPresetFolder().getFullPathName().toStdString());
    }

    // The one-time copy, in a scratch ~/Library.
    const auto lib = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("dc-migration-test");
    lib.deleteRecursively();
    const auto old = oldAppDataIn(lib);
    old.getChildFile("Presets/My Crypt.dcpreset").create();
    old.getChildFile("Presets/My Crypt.dcpreset").replaceWithText("<DUNGEON_CRAWLER/>");
    old.getChildFile("Presets/Kept.dcpreset").create();
    old.getChildFile("Presets/Kept.dcpreset").replaceWithText("old");
    old.getChildFile("scenes/map.png").create();
    const auto host = lib.getChildFile("Audio/Presets/Veil Audio");
    host.getChildFile("DUNGEON CRAWLER/Strings.aupreset").create();
    host.getChildFile("DUNGEON CRAWLER/Strings.aupreset").replaceWithText(aupreset("1147626345"));   // Dgci
    host.getChildFile("DUNGEON CRAWLER/Old Effect.aupreset").create();
    host.getChildFile("DUNGEON CRAWLER/Old Effect.aupreset").replaceWithText(aupreset("1147626354"));   // Dgcr
    host.getChildFile("DUNGEON CRAWLER FX/Echo.aupreset").create();
    host.getChildFile("DUNGEON CRAWLER FX/Echo.aupreset").replaceWithText("unreadable");
    // A file already in the new place is never overwritten.
    dc::userdata::presetsFolder(true, lib).getChildFile("Kept.dcpreset").create();
    dc::userdata::presetsFolder(true, lib).getChildFile("Kept.dcpreset").replaceWithText("new");

    const auto mi = dc::userdata::migrate(true, lib);
    const auto mf = dc::userdata::migrate(false, lib);
    s.check(mi.ran && mi.complete && mf.ran && mf.complete, "migration runs and completes for both plug-ins",
            std::to_string(mi.copied) + " + " + std::to_string(mf.copied) + " copied");
    for (bool instrument : { true, false }) {
        const auto label = std::string(instrument ? " (instrument)" : " (effect)");
        s.check(dc::userdata::presetsFolder(instrument, lib).getChildFile("My Crypt.dcpreset").loadFileAsString() == "<DUNGEON_CRAWLER/>",
                "your presets are copied to the new folder" + label);
        s.check(dc::userdata::scenesFolder(instrument, lib).getChildFile("map.png").existsAsFile(), "drop-in scenes are copied" + label);
        s.check(dc::userdata::pluginFolder(instrument, lib).getChildFile(dc::userdata::markerName).existsAsFile(), "the copy is marked done" + label);
    }
    s.check(dc::userdata::presetsFolder(true, lib).getChildFile("Kept.dcpreset").loadFileAsString() == "new"
         && dc::userdata::presetsFolder(false, lib).getChildFile("Kept.dcpreset").loadFileAsString() == "old",
            "a preset already in the new folder is kept, not overwritten");
    const auto newHost = lib.getChildFile("Audio/Presets/Windward Audio");
    s.check(newHost.getChildFile("DUNGEON CRAWLER/Strings.aupreset").existsAsFile()
         && newHost.getChildFile("DUNGEON CRAWLER FX/Old Effect.aupreset").existsAsFile()
         && newHost.getChildFile("DUNGEON CRAWLER FX/Echo.aupreset").existsAsFile()
         && !newHost.getChildFile("DUNGEON CRAWLER/Old Effect.aupreset").exists()
         && !newHost.getChildFile("DUNGEON CRAWLER FX/Strings.aupreset").exists(),
            "host presets go to Windward Audio/<plug-in>, each to the plug-in it was saved with");
    s.check(old.getChildFile("Presets/My Crypt.dcpreset").existsAsFile() && old.getChildFile("scenes/map.png").existsAsFile()
         && host.getChildFile("DUNGEON CRAWLER/Strings.aupreset").existsAsFile(),
            "the old folders are kept as they were (a copy)");
    old.getChildFile("Presets/Later.dcpreset").create();
    const auto second = dc::userdata::migrate(true, lib);
    s.check(!second.ran && !dc::userdata::presetsFolder(true, lib).getChildFile("Later.dcpreset").exists(), "the copy happens once");
    const auto none = dc::userdata::migrate(true, lib.getChildFile("empty"));
    s.check(!none.ran && !lib.getChildFile("empty").exists(), "nothing old: nothing is created");
    lib.deleteRecursively();

    // Sessions saved before 1.0 (the parameter IDs and the state format are unchanged).
    checkOldSession(s, PluginKind::Effect, "session-pre-1.0-fx.xml");
    checkOldSession(s, PluginKind::Instrument, "session-pre-1.0-instrument.xml");
}
