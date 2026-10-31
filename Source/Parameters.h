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
inline juce::String advanced(std::size_t index) { return juce::String("adv_") + moduleSpecs[index].id; }
}

namespace dc {
// Travel Sync choices: Free (Travel Time in seconds) or a length in beats.
inline constexpr std::array<double, 6> travelSyncBeats { 0, 1, 2, 4, 8, 16 };
inline constexpr const char* travelSyncChoices = "Free|1/4|1/2|1 bar|2 bars|4 bars";

inline juce::StringArray advancedChoices(const ModuleParamSpec& spec) {
    juce::StringArray choices { "Place" };
    const auto named = juce::StringArray::fromTokens(spec.choices, "|", "");
    if (named.size() > 0 && juce::String(spec.choices).isNotEmpty()) choices.addArray(named);
    else for (int v = static_cast<int>(spec.min); v <= static_cast<int>(spec.max); ++v) choices.add(juce::String(v));
    return choices;
}

// `currentLocation` names the sub-locations of the location in use (for host display).
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(std::function<int()> currentLocation) {
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
    return layout;
}
}
