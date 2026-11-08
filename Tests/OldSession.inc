//==============================================================================
#include <cstring>
// An old session (the company rename): sessions of both plug-ins saved by the build
// before the rename (commit fb988b1, Tests/fixtures/old-session-{instrument,fx}.dcstate,
// written by --write-old-session with that build) must load unchanged: the AU codes,
// the parameter IDs and the state format did not change, only the names hosts and
// users see. "Unchanged": loaded and saved again, each gives exactly the state that
// build gave when it loaded and saved it (<fixture>.expected.xml, written with it).
namespace oldSession {
void setParam(DungeonCrawlerProcessor& p, const juce::String& id, float plain) {
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}

// A session with some of everything: a place, the controls, a route, the window.
void make(DungeonCrawlerProcessor& p) {
    p.setRateAndBufferSizeDetails(48000, 512);
    p.prepareToPlay(48000, 512);
    setParam(p, dc::ids::location, 3.0f);
    setParam(p, dc::ids::subLocation, 2.0f);
    setParam(p, dc::ids::intensity, 0.7f);
    setParam(p, dc::ids::mix, 0.6f);
    setParam(p, dc::ids::travelTime, 3.5f);
    setParam(p, dc::ids::macros[1], 0.8f);
    setParam(p, dc::ids::atmosLevel, 0.4f);
    setParam(p, dc::ids::roamSeed, 17.0f);
    setParam(p, dc::ids::mixGroups[0], 1.2f);
    setParam(p, dc::ids::mixLowCut, 180.0f);
    if (p.isInstrument()) {
        setParam(p, dc::ids::instMacros[2], 0.3f);
        setParam(p, dc::ids::instLevel, 0.9f);
        setParam(p, dc::ids::bendRange, 7.0f);
    }
    p.setRoute({ { 1, 0 }, { 3, 2 }, { 0, 4 } });
    p.updateViewState([](DungeonCrawlerProcessor::ViewState& v) { v.valid = true; v.place = true; v.location = 3; v.sub = 2; v.advanced = true; });
}

// The state's XML text as saved (JUCE's binary XML: a magic number, the text's size, the
// text), without the diagnostic `samples` count (how many recorded notes this build has;
// never read back). The AU host harnesses compare the same way.
juce::String xmlOf(const juce::MemoryBlock& state) {
    if (state.getSize() <= 8) return {};
    const auto* bytes = static_cast<const char*>(state.getData());
    uint32_t magic = 0, size = 0;
    std::memcpy(&magic, bytes, 4);
    std::memcpy(&size, bytes + 4, 4);
    if (magic != 0x21324356u || size_t(size) + 8 > state.getSize()) return {};
    std::string xml(bytes + 8, size);
    while (!xml.empty() && xml.back() == '\0') xml.pop_back();
    const auto at = xml.find(" samples=\"");
    if (at != std::string::npos) {
        const auto end = xml.find('"', at + 10);
        if (end != std::string::npos) xml.erase(at, end + 1 - at);
    }
    return juce::String::fromUTF8(xml.data(), int(xml.size()));
}
juce::String xmlOf(DungeonCrawlerProcessor& p) {
    juce::MemoryBlock state;
    p.getStateInformation(state);
    return xmlOf(state);
}

int write(const juce::String& folder) {
    const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile(folder);
    dir.createDirectory();
    for (const bool instrument : { true, false }) {
        DungeonCrawlerProcessor p(instrument ? PluginKind::Instrument : PluginKind::Effect);
        make(p);
        juce::MemoryBlock state;
        p.getStateInformation(state);
        const auto file = dir.getChildFile(instrument ? "old-session-instrument.dcstate" : "old-session-fx.dcstate");
        if (!file.replaceWithData(state.getData(), state.getSize())) return 1;
        std::printf("wrote %s (%d bytes)\n", file.getFullPathName().toRawUTF8(), int(state.getSize()));
        // What this build saves after loading it (the comparison for later builds).
        DungeonCrawlerProcessor back(instrument ? PluginKind::Instrument : PluginKind::Effect);
        back.setStateInformation(state.getData(), int(state.getSize()));
        const auto text = xmlOf(back).toStdString();
        if (!file.getSiblingFile(file.getFileName() + ".expected.xml").replaceWithData(text.data(), text.size())) return 1;
    }
    return 0;
}

// The rename's checks: the names hosts show, the old sessions loading unchanged, and the
// user's files copied from the old folders into each plug-in's own.
void test(Suite& s) {
    s.section("company rename");
    // What hosts list (the AU's name is "<manufacturer>: <name>"; this binary is built
    // with the effect's identity, the instrument's is checked in the bundles), and what stays.
#define DC_RENAME_TEXT2(x) #x
#define DC_RENAME_TEXT(x) DC_RENAME_TEXT2(x)
    s.check(juce::String(JucePlugin_Manufacturer) + ": " + JucePlugin_Name == "Windward Audio: DUNGEON CRAWLER FX", "hosts list the effect as Windward Audio: DUNGEON CRAWLER FX",
            juce::String(JucePlugin_Manufacturer).toStdString());
    const juce::String bundleId(DC_RENAME_TEXT(JucePlugin_CFBundleIdentifier));
    s.check(bundleId == "com.windwardaudio.dungeoncrawlerfx", "the effect's bundle ID is com.windwardaudio.dungeoncrawlerfx", bundleId.toStdString());
    s.check(JucePlugin_ManufacturerCode == 0x5665696c && JucePlugin_PluginCode == 0x44676372, "the effect's AU codes are unchanged (manufacturer Veil, subtype Dgcr)");
    {
        DungeonCrawlerProcessor inst(PluginKind::Instrument), fx(PluginKind::Effect);
        s.check(inst.getName() == "DUNGEON CRAWLER" && fx.getName() == "DUNGEON CRAWLER FX", "the plug-ins keep their names");
    }

    for (const bool instrument : { true, false }) {
        const std::string which = instrument ? "the instrument" : "the effect";
        const auto file = juce::File(DC_SOURCE_DIR).getChildFile(instrument ? "Tests/fixtures/old-session-instrument.dcstate" : "Tests/fixtures/old-session-fx.dcstate");
        juce::MemoryBlock old;
        const bool readable = file.loadFileAsData(old) && old.getSize() > 8;
        s.check(readable, "the old session of " + which + " is readable", file.getFullPathName().toStdString());
        if (!readable) continue;
        const auto expected = file.getSiblingFile(file.getFileName() + ".expected.xml").loadFileAsString();
        DungeonCrawlerProcessor b(instrument ? PluginKind::Instrument : PluginKind::Effect);
        b.setStateInformation(old.getData(), int(old.getSize()));
        const auto back = xmlOf(b);
        int differ = 0;
        while (differ < expected.length() && differ < back.length() && expected[differ] == back[differ]) ++differ;
        s.check(expected.isNotEmpty() && back == expected, "an old session of " + which + " loads unchanged (its state compared with the state saved again)",
                "first difference at character " + std::to_string(differ) + ": " + back.substring(std::max(0, differ - 60), differ + 60).toStdString());
        int mismatches = 0, found = 0;
        std::string first;
        if (const auto xml = juce::AudioProcessor::getXmlFromBinary(old.getData(), int(old.getSize())))
            for (const auto* param : xml->getChildWithTagNameIterator("PARAM")) {
                const auto id = param->getStringAttribute("id");
                auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(b.parameters.getParameter(id));
                if (parameter == nullptr) { if (mismatches++ == 0) first = id.toStdString() + " (no such parameter)"; continue; }
                ++found;
                if (std::abs(parameter->convertFrom0to1(parameter->getValue()) - float(param->getDoubleAttribute("value"))) > 1.0e-4f && mismatches++ == 0) first = id.toStdString();
            }
        s.check(found > 0 && found == b.getParameters().size() && mismatches == 0, "every parameter ID of " + which + "'s old session exists and has its saved value",
                std::to_string(found) + " of " + std::to_string(b.getParameters().size()) + ", " + std::to_string(mismatches) + " differ " + first);
        DungeonCrawlerProcessor now(instrument ? PluginKind::Instrument : PluginKind::Effect);
        make(now);
        s.check(xmlOf(now) == xmlOf(old), "the same session of " + which + " made with this build saves the same state as the build before the rename");
    }

    // The user's files: copied once from the old folders into each plug-in's own, the
    // originals kept; each host preset goes to the plug-in its type names.
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("dc-rename-test");
    root.deleteRecursively();
    namespace ud = dc::userdata;
    const auto oldPreset = ud::oldFolder(root).getChildFile("Presets/Crypt Echo.dcpreset");
    const auto oldScene = ud::oldFolder(root).getChildFile("scenes/castle/chapel.png");
    const auto aupreset = [&root](const char* folder, const char* name, juce::uint32 type) {
        const auto f = root.getChildFile("Audio/Presets/Veil Audio").getChildFile(folder).getChildFile(name);
        f.getParentDirectory().createDirectory();
        f.replaceWithText("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\"><dict><key>name</key><string>x</string><key>type</key><integer>"
                          + juce::String(juce::int64(type)) + "</integer></dict></plist>\n");
        return f;
    };
    for (const auto& f : { oldPreset, oldScene }) {
        f.getParentDirectory().createDirectory();
        f.replaceWithText("old " + f.getFileName());
    }
    const auto instHost = aupreset("DUNGEON CRAWLER", "Bells.aupreset", 0x61756d75u);
    const auto fxHost = aupreset("DUNGEON CRAWLER FX", "Cave.aupreset", 0x61756678u);
    const auto oldestHost = aupreset("DungeonCrawler", "Oldest.aupreset", 0x61756678u);
    const auto misfiled = aupreset("DUNGEON CRAWLER FX", "Misfiled.aupreset", 0x61756d75u);   // an instrument preset in the effect's folder
    s.check(ud::presetKind(instHost) == 1 && ud::presetKind(fxHost) == 0, "an .aupreset's Audio Unit type is read");
    const auto i1 = ud::migrate(true, root), f1 = ud::migrate(false, root);
    s.check(ud::folder(true, root) == root.getChildFile("Windward Audio/DUNGEON CRAWLER") && ud::folder(false, root) == root.getChildFile("Windward Audio/DUNGEON CRAWLER FX"),
            "each plug-in's files live in ~/Library/Windward Audio/<plug-in>");
    s.check(i1.ran && f1.ran && i1.copied == 4 && f1.copied == 4, "each plug-in's first launch copies the old folders' files",
            std::to_string(i1.copied) + " and " + std::to_string(f1.copied) + " copied");
    bool presetsBoth = true;
    for (const bool instrument : { true, false })
        presetsBoth = presetsBoth && ud::presets(instrument, root).getChildFile("Crypt Echo.dcpreset").loadFileAsString() == "old Crypt Echo.dcpreset"
                      && ud::scenes(instrument, root).getChildFile("castle/chapel.png").existsAsFile();
    s.check(presetsBoth, "both plug-ins get the user presets and drop-in scenes they shared");
    s.check(ud::hostPresets(true, root).getChildFile("Bells.aupreset").existsAsFile() && ud::hostPresets(true, root).getChildFile("Misfiled.aupreset").existsAsFile()
            && !ud::hostPresets(true, root).getChildFile("Cave.aupreset").exists()
            && ud::hostPresets(false, root).getChildFile("Cave.aupreset").existsAsFile() && ud::hostPresets(false, root).getChildFile("Oldest.aupreset").existsAsFile()
            && !ud::hostPresets(false, root).getChildFile("Bells.aupreset").exists() && !ud::hostPresets(false, root).getChildFile("Misfiled.aupreset").exists(),
            "each host preset goes to the plug-in its Audio Unit type names");
    s.check(root.getChildFile("Audio/Presets/Windward Audio/DUNGEON CRAWLER/Bells.aupreset").existsAsFile()
            && root.getChildFile("Audio/Presets/Windward Audio/DUNGEON CRAWLER FX/Cave.aupreset").existsAsFile(),
            "and into the host's folder under the new company name");
    s.check(oldPreset.existsAsFile() && oldScene.existsAsFile() && instHost.existsAsFile() && fxHost.existsAsFile() && oldestHost.existsAsFile() && misfiled.existsAsFile(),
            "the originals are kept");
    oldPreset.getSiblingFile("Later.dcpreset").replaceWithText("later");
    const auto i2 = ud::migrate(true, root);
    s.check(!i2.ran && !ud::presets(true, root).getChildFile("Later.dcpreset").exists(), "the copy runs once");
    {
        DungeonCrawlerProcessor p(PluginKind::Instrument), fx(PluginKind::Effect);
        s.check(p.userPresetFolder() == ud::presets(true) && fx.userPresetFolder() == ud::presets(false), "user presets are saved in the plug-in's new folder",
                p.userPresetFolder().getFullPathName().toStdString());
    }
    root.deleteRecursively();
}
}   // namespace oldSession
