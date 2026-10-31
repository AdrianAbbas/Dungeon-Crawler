#pragma once
// Host parameters. IDs are stable and versioned (ParameterID{id, 1}); JUCE hashes the
// string IDs into AU parameter IDs, so parameters added in later milestones never
// disturb saved automation. Location order is fixed (automation stores the index).
#include <juce_audio_processors/juce_audio_processors.h>
#include "World/Locations.h"

namespace dc::ids {
inline constexpr const char* location = "location";
inline constexpr const char* intensity = "intensity";
inline constexpr const char* mix = "mix";
inline constexpr const char* input = "input";
inline constexpr const char* output = "output";
inline constexpr const char* bypass = "bypass";
}

namespace dc {
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
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
    return layout;
}
}
