#pragma once
// Host parameters. IDs are stable and versioned (ParameterID{id, 1}); JUCE hashes the
// string IDs into AU parameter IDs, so parameters added in later milestones never
// disturb saved automation. Location order is fixed (automation stores the index).
//
//   Location, Sub-location        where you are (both automatable)
//   Intensity, Mix, Input, Output the main controls
//   Travel Time, Travel Sync      how long a journey takes (free seconds or bars)
//   Macro 1..4                    the location's themed macros (0.5 = as designed)
//   Adv ...                       the Advanced drawer: one per module parameter
//   Bypass                        the host bypass
#include <juce_audio_processors/juce_audio_processors.h>
#include "World/Locations.h"
#include <functional>

namespace dc::ids {
inline constexpr const char* location = "location";
inline constexpr const char* subLocation = "subLocation";
inline constexpr const char* intensity = "intensity";
inline constexpr const char* mix = "mix";
inline constexpr const char* input = "input";
inline constexpr const char* output = "output";
inline constexpr const char* bypass = "bypass";
inline constexpr const char* travelTime = "travelTime";
inline constexpr const char* travelSync = "travelSync";
inline constexpr std::array<const char*, macroCount> macros { "macro1", "macro2", "macro3", "macro4" };
// Milestone 9: journeys, roaming, MIDI travel.
inline constexpr const char* journeyOn = "journeyOn";
inline constexpr const char* journeyMode = "journeyMode";
inline constexpr const char* journeyBars = "journeyBars";
inline constexpr const char* roamOn = "roamOn";
inline constexpr const char* roamBars = "roamBars";
inline constexpr const char* roamRandomness = "roamRandomness";
inline constexpr const char* roamSeed = "roamSeed";
inline constexpr const char* midiTravel = "midiTravel";
inline constexpr const char* midiOctave = "midiOctave";
// Milestone 10: the Atmosphere layer.
inline constexpr const char* atmosLevel = "atmosLevel";
inline constexpr const char* atmosDensity = "atmosDensity";
inline constexpr const char* atmosSeed = "atmosSeed";
// Milestone 11: Eco / Standard / High (a setting, not automatable).
inline constexpr const char* quality = "quality";
inline juce::String advanced(std::size_t index) { return juce::String("adv_") + moduleSpecs[index].id; }
// Direction Update 3: the instrument (DUNGEON CRAWLER only; the FX has none of these).
inline constexpr const char* instrument = "instrument";
inline constexpr const char* instLock = "instLock";
inline constexpr std::array<const char*, 5> instMacros { "instMacro1", "instMacro2", "instMacro3", "instMacro4", "instMacro5" };
inline constexpr const char* bendRange = "bendRange";
// Session 9 (appended, so existing parameters keep their places): the Effects roller (both
// plug-ins) and the instrument's Level, Vowel and Ensemble.
inline constexpr const char* effects = "effects";
inline constexpr const char* instLevel = "instLevel";
inline constexpr const char* instVowel = "instVowel";
inline constexpr const char* instEnsemble = "instEnsemble";
// Session 11: the MIX section (both plug-ins; 100 % = as designed), the reverb helpers,
// the Atmosphere switch, and the instrument's Punch.
inline constexpr std::array<const char*, 5> mixGroups { "mixReverb", "mixEchoes", "mixModulation", "mixCharacter", "mixAtmosphere" };
inline constexpr std::array<const char*, 5> mixGroupNames { "Reverb / Space", "Echoes", "Modulation", "Character", "Atmosphere" };
inline constexpr const char* mixLowCut = "mixLowCut";
inline constexpr const char* mixPreDelay = "mixPreDelay";
inline constexpr const char* mixDuck = "mixDuck";
inline constexpr const char* atmosOn = "atmosOn";
inline constexpr const char* instPunch = "instPunch";
inline constexpr const char* instPunchTime = "instPunchTime";
}

namespace dc {
inline constexpr float effectsMax = 1.5f;   // the Effects roller's top (150 %)
inline constexpr float mixMax = 1.5f;        // the MIX sliders' top (150 %)
inline constexpr float mixLowCutOff = 20.0f; // Reverb Low Cut at its bottom: off
// Travel Sync choices: Free (Travel Time in seconds) or a length in beats.
inline constexpr std::array<double, 6> travelSyncBeats { 0, 1, 2, 4, 8, 16 };
inline constexpr const char* travelSyncChoices = "Free|1/4|1/2|1 bar|2 bars|4 bars";
inline constexpr const char* journeyModeChoices = "Loop|One-way|Ping-pong";
// Roam: how long the plug-in stays in a sub-location before wandering on.
inline constexpr std::array<double, 6> roamBarChoices { 0.5, 1, 2, 4, 8, 16 };
inline constexpr const char* roamBarNames = "1/2 bar|1 bar|2 bars|4 bars|8 bars|16 bars";
// MIDI travel: the octave whose notes choose places (C = first location ... A = tenth;
// in the octave above, C..E choose the sub-location). Octave numbers as in most DAWs
// (middle C = C3 here, MIDI note 60).
inline constexpr const char* midiOctaveNames = "C-2|C-1|C0|C1|C2|C3|C4|C5|C6";

inline juce::StringArray advancedChoices(const ModuleParamSpec& spec) {
    juce::StringArray choices { "Place" };
    const auto named = juce::StringArray::fromTokens(spec.choices, "|", "");
    if (named.size() > 0 && juce::String(spec.choices).isNotEmpty()) choices.addArray(named);
    else for (int v = static_cast<int>(spec.min); v <= static_cast<int>(spec.max); ++v) choices.add(juce::String(v));
    return choices;
}

// `currentLocation` names the sub-locations of the location in use (for host display).
// `instrumentNames` (the instrument's, in place order) adds the instrument's parameters:
// the plug-in is then DUNGEON CRAWLER, the instrument; empty, it is DUNGEON CRAWLER FX.
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(std::function<int()> currentLocation,
                                                                                const juce::StringArray& instrumentNames = {}) {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray places;
    for (const auto* name : locationNames) places.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::location, 1 }, "Location", places, 0));
    const auto percent = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() / 100.0f; });
    const auto decibels = juce::AudioParameterFloatAttributes().withLabel("dB")
        .withStringFromValueFunction([](float v, int) { return juce::String(v, 1); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::intensity, 1 }, "Intensity",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mix, 1 }, "Mix",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::input, 1 }, "Input",
                                                           juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, decibels));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::output, 1 }, "Output",
                                                           juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f, decibels));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    // Milestone 1.
    auto subNames = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([currentLocation](int v, int) { return juce::String(subLocationName(currentLocation(), v)); })
        .withValueFromStringFunction([currentLocation](const juce::String& s) {
            const int location = currentLocation();
            for (int k = 0; k <= subLocationCount(location); ++k)
                if (s.trim().equalsIgnoreCase(subLocationName(location, k))) return k;
            return s.getIntValue();
        });
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::subLocation, 1 }, "Sub-location",
                                                         0, maxSubLocations, 0, subNames));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::travelTime, 1 }, "Travel Time",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.0f, 0.4f), 1.5f,
        juce::AudioParameterFloatAttributes().withLabel("s")
            .withStringFromValueFunction([](float v, int) { return v < 0.02f ? juce::String("Instant") : juce::String(v, v < 1.0f ? 2 : 1) + " s"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.trim().equalsIgnoreCase("Instant") ? 0.0f : s.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::travelSync, 1 }, "Travel Sync",
                                                            juce::StringArray::fromTokens(travelSyncChoices, "|", ""), 0));
    const auto macroText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { const int p = juce::roundToInt((v - 0.5f) * 200.0f); return (p > 0 ? "+" : "") + juce::String(p) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return 0.5f + s.getFloatValue() / 200.0f; });
    for (int m = 0; m < macroCount; ++m)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::macros[static_cast<std::size_t>(m)], 1 },
                                                               "Macro " + juce::String(m + 1), juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
    // Milestone 9: the journey path (a route of places, bars per stop), roaming between
    // a location's places, MIDI travel. All automatable; the route itself is state.
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::journeyOn, 1 }, "Journey", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::journeyMode, 1 }, "Journey Mode",
                                                            juce::StringArray::fromTokens(journeyModeChoices, "|", ""), 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::journeyBars, 1 }, "Bars per Stop", 1, 16, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v) + (v == 1 ? " bar" : " bars"); })));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::roamOn, 1 }, "Roam", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::roamBars, 1 }, "Roam Rate",
                                                            juce::StringArray::fromTokens(roamBarNames, "|", ""), 2));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::roamRandomness, 1 }, "Roam Randomness",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, percent));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::roamSeed, 1 }, "Roam Seed", 1, 999, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::midiTravel, 1 }, "MIDI Travel", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::midiOctave, 1 }, "MIDI Travel Octave",
                                                            juce::StringArray::fromTokens(midiOctaveNames, "|", ""), 1));

    // Milestone 10: the Atmosphere layer (off by default).
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::atmosLevel, 1 }, "Atmosphere",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::atmosDensity, 1 }, "Atmosphere Density",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f, percent));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::atmosSeed, 1 }, "Atmosphere Seed", 1, 999, 1));

    // Milestone 11: quality mode. Eco: the Wraith in mono, no animation, art at 1x;
    // Standard: full DSP, gentle animation; High: smoother animation and art.
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::quality, 1 }, "Quality",
                                                            juce::StringArray { "Eco", "Standard", "High" }, 1,
                                                            juce::AudioParameterChoiceAttributes().withAutomatable(false)));

    // The Advanced drawer: an offset from the place for every continuous module
    // parameter (0 = as designed), an override for every choice ("Place" = as designed).
    const auto offsetText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { const int p = juce::roundToInt(v * 100.0f); return (p > 0 ? "+" : "") + juce::String(p) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() / 100.0f; });
    for (std::size_t i = 0; i < Count; ++i) {
        const auto& spec = moduleSpecs[i];
        const juce::ParameterID id { ids::advanced(i), 1 };
        const juce::String name = juce::String("Adv ") + spec.name;
        if (spec.glide == Glide::Step) layout.add(std::make_unique<juce::AudioParameterChoice>(id, name, advancedChoices(spec), 0));
        else layout.add(std::make_unique<juce::AudioParameterFloat>(id, name, juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f, offsetText));
    }

    if (!instrumentNames.isEmpty()) {
        // The instrument the place gives (Lock Instrument off), or this one (on).
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::instLock, 1 }, "Lock Instrument", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::instrument, 1 }, "Instrument", instrumentNames, 0));
        // The instrument's five macros (their names change with the instrument: Tone,
        // Attack, Release, Body and its own signature control).
        for (std::size_t m = 0; m < ids::instMacros.size(); ++m)
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instMacros[m], 1 }, "Instrument Macro " + juce::String(int(m) + 1),
                                                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::bendRange, 1 }, "Pitch Bend Range", 1, 24, 2,
            juce::AudioParameterIntAttributes().withLabel("st")
                .withStringFromValueFunction([](int v, int) { return juce::String(v) + " st"; })
                .withAutomatable(false)));
        // Session 9: Level (dB) and the voices' Vowel and Ensemble (50 %: as designed).
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instLevel, 1 }, "Instrument Level",
                                                               juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f, decibels));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instVowel, 1 }, "Vowel",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instEnsemble, 1 }, "Ensemble",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
    }
    // Session 9: the Effects roller. 100 % is the place as designed (and what a session
    // saved before it existed opens at); 0 % nearly the dry signal; 150 % stronger.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::effects, 1 }, "Effects",
                                                           juce::NormalisableRange<float>(0.0f, effectsMax), 1.0f, percent));
    // Session 11 (appended: no existing ID moves; a session saved before opens at these
    // defaults, which leave every sound exactly as it was).
    for (std::size_t g = 0; g < ids::mixGroups.size(); ++g)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixGroups[g], 1 }, juce::String("Mix ") + ids::mixGroupNames[g],
                                                               juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    juce::NormalisableRange<float> lowCut(mixLowCutOff, 600.0f);
    lowCut.setSkewForCentre(150.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixLowCut, 1 }, "Reverb Low Cut", lowCut, mixLowCutOff,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
            .withStringFromValueFunction([](float v, int) { return v <= mixLowCutOff + 0.5f ? juce::String("Off") : juce::String(juce::roundToInt(v)) + " Hz"; })
            .withValueFromStringFunction([](const juce::String& t) { return t.trim().equalsIgnoreCase("off") ? mixLowCutOff : t.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixPreDelay, 1 }, "Reverb Pre-delay",
        juce::NormalisableRange<float>(-40.0f, 120.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
            .withStringFromValueFunction([](float v, int) { const int ms = juce::roundToInt(v); return (ms > 0 ? "+" : "") + juce::String(ms) + " ms"; })
            .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixDuck, 1 }, "Reverb Duck",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::atmosOn, 1 }, "Atmosphere On", true));
    if (!instrumentNames.isEmpty()) {
        // Punch: 100 % is the instrument's own (high on the brass and drums, low or none
        // elsewhere), 200 % +12 dB on any instrument, 0 % none.
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instPunch, 1 }, "Punch",
                                                               juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f, percent));
        juce::NormalisableRange<float> punchTime(20.0f, 500.0f);
        punchTime.setSkewForCentre(120.0f);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instPunchTime, 1 }, "Punch Time", punchTime, 120.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")
                .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; })
                .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); })));
    }
    return layout;
}
}
