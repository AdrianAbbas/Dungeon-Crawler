#pragma once
// The internal parameters of the six sound modules imported from VEIL. They are not
// host parameters: a location (and later a sub-location, the macros and the Advanced
// drawer) produces a Frame of these values, and the engine interpolates frames while
// travelling. Index order is the Frame order used everywhere.
#include <array>
#include <cstddef>

namespace dc {
enum P : std::size_t {
    // I. The Abyss: filters, Darkness, Movement, Tomb Resonator.
    Cutoff, Resonance, HighPass, Slope, Darkness, FilterMovement, FilterRate, FilterSync,
    Tomb, TombNote, TombOctave, TombRing,
    // II. Ancient Blood: saturation and degradation.
    Drive, Texture, Age, Dust, Instability, BloodMix,
    // III. The Choir: granular pitch voices and Drone.
    Voices, Interval, ChoirDetune, Formant, ChoirMovement, ChoirBlend, Drone, DroneHold,
    // IV. The Wraith: spectral blur, ghost and freeze.
    Blur, WraithDetune, Drift, Ghost, Freeze, WraithMix,
    // V. The Echoes: delays incl. reverse and descent.
    DelayTime, DelaySync, Feedback, DelayDamping, Diffusion, Reverse, Descent, Spread, DelayMix,
    // VI. The Crypt: FDN reverb.
    Space, Decay, PreDelay, Damping, ReverbLowCut, Shimmer, ReverbWidth, ReverbMix, Character,
    CryptMode, RiseTime, Eternal,
    // VII. Relics: torch flicker, struck metal, wind and drips (DUNGEON CRAWLER).
    Flicker, FlickerRate, Metal, MetalKind, MetalNote, MetalOctave, MetalDecay, Wind, Drips,
    // VIII. Signature: the one effect that belongs to a single place (Milestone 6).
    SigKind, SigAmount, SigTone, SigRate,
    Count
};
using Frame = std::array<float, Count>;

// Interpolation: Lin (straight), Log (geometric, for frequencies and times) or Step
// (a choice or switch: the value changes once, half-way).
enum class Glide { Lin, Log, Step };
struct ModuleParamSpec { const char* id; const char* name; float min, max, neutral; Glide glide; const char* choices; };

// The signature effects (VIII), one per place, in kind order (0 = none). See
// Source/DSP/Signature.cpp for what each one does.
inline constexpr const char* signatureChoices =
    "None|Receding Footsteps|Stone Flutter|Bar Resonance|Glitter Sparkle|Sub Choir Menace"
    "|Wall Ping-Pong|Banner Reflections|Regal Brass|Choir Pad Bloom|Wind-Blown Echoes"
    "|Colonnade|Vault Cascade|Stone Resonance|Bell Resonators"
    "|Trunk Scatter|Spectral Smear|Crooked Wobble|Breathing Shimmer"
    "|Cave Hum|Glass Ring|Underwater|Growl"
    "|Folk Chorus|Warm Tavern|Slap-Back|Anvil"
    "|Mud Drag|Murky Wobble|Sloshing"
    "|Valley Answer|Canyon Echoes|Icy Shimmer|Forge Resonance"
    "|Iron Groan|Reverse Swell|Hollow Tomb"
    "|Spiral Echoes|Paper Hush|Bubbling Alchemy|Starry Shimmer";

// `neutral` is the transparent setting: every module at its neutral values leaves the
// signal unchanged (the Abyss open, no drive, every wet layer at zero).
inline constexpr std::array<ModuleParamSpec, Count> moduleSpecs {{
    { "cutoff", "Cutoff", 40, 20000, 20000, Glide::Log, "" },
    { "resonance", "Resonance", 0, 1, 0, Glide::Lin, "" },
    { "highPass", "High-Pass", 20, 2000, 20, Glide::Log, "" },
    { "slope", "Slope", 0, 1, 0, Glide::Step, "12 dB|24 dB" },
    { "darkness", "Darkness", 0, 1, 0, Glide::Lin, "" },
    { "filterMovement", "Movement", 0, 1, 0, Glide::Lin, "" },
    { "filterRate", "Movement Rate", .01f, 8, .12f, Glide::Log, "" },
    { "filterSync", "Movement Sync", 0, 5, 0, Glide::Step, "Free|1/4|1/2|1 bar|2 bars|4 bars" },
    { "tomb", "Tomb Resonator", 0, 1, 0, Glide::Lin, "" },
    { "tombNote", "Tomb Note", 0, 11, 2, Glide::Step, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
    { "tombOctave", "Tomb Octave", 0, 4, 1, Glide::Step, "1|2|3|4|5" },
    { "tombRing", "Tomb Ring", 0, 1, .65f, Glide::Lin, "" },

    { "drive", "Drive", 0, 36, 0, Glide::Lin, "" },
    { "texture", "Texture", 0, 1, .35f, Glide::Lin, "" },
    { "age", "Age", 0, 1, 0, Glide::Lin, "" },
    { "dust", "Dust", 0, 1, 0, Glide::Lin, "" },
    { "instability", "Instability", 0, 1, 0, Glide::Lin, "" },
    { "bloodMix", "Blood Mix", 0, 1, 0, Glide::Lin, "" },

    { "voices", "Voice Count", 1, 4, 2, Glide::Step, "" },
    { "interval", "Interval", 0, 5, 0, Glide::Step, "Octave below|Fifth below|Fourth above|Fifth above|Octave above|Octave + fifth" },
    { "choirDetune", "Choir Detune", 0, 50, 12, Glide::Lin, "" },
    { "formant", "Formant", 0, 1, .4f, Glide::Lin, "" },
    { "choirMovement", "Choir Movement", 0, 1, .35f, Glide::Lin, "" },
    { "choirBlend", "Choir Blend", 0, 1, 0, Glide::Lin, "" },
    { "drone", "Drone", 0, 1, 0, Glide::Lin, "" },
    { "droneHold", "Drone Hold", .5f, 30, 6, Glide::Log, "" },

    { "blur", "Spectral Blur", 0, 1, .5f, Glide::Lin, "" },
    { "wraithDetune", "Wraith Detune", 0, 100, 15, Glide::Lin, "" },
    { "drift", "Drift", 0, 1, .4f, Glide::Lin, "" },
    { "ghost", "Ghost", 0, 1, .3f, Glide::Lin, "" },
    { "freeze", "Freeze", 0, 1, 0, Glide::Step, "Off|On" },
    { "wraithMix", "Wraith Mix", 0, 1, 0, Glide::Lin, "" },

    { "delayTime", "Time", 20, 2000, 400, Glide::Log, "" },
    { "delaySync", "Time Sync", 0, 7, 0, Glide::Step, "Free|1/16|1/8|1/8 dotted|1/4|1/4 dotted|1/2|1 bar" },
    { "feedback", "Feedback", 0, .98f, .3f, Glide::Lin, "" },
    { "delayDamping", "Echo Damping", 0, 1, .5f, Glide::Lin, "" },
    { "diffusion", "Diffusion", 0, 1, .35f, Glide::Lin, "" },
    { "reverse", "Reverse", 0, 1, 0, Glide::Step, "Off|On" },
    { "descent", "Descent", 0, 3, 0, Glide::Step, "Off|Semitone|Fifth|Octave" },
    { "spread", "Stereo Spread", 0, 1, .6f, Glide::Lin, "" },
    { "delayMix", "Echoes Mix", 0, 1, 0, Glide::Lin, "" },

    { "space", "Space", 0, 1, .5f, Glide::Lin, "" },
    { "decay", "Decay", .3f, 30, 2, Glide::Log, "" },
    { "preDelay", "Pre-Delay", 0, 250, 20, Glide::Lin, "" },
    { "damping", "Damping", 0, 1, .5f, Glide::Lin, "" },
    { "reverbLowCut", "Low Cut", 20, 1000, 100, Glide::Log, "" },
    { "shimmer", "Shimmer", 0, 1, 0, Glide::Lin, "" },
    { "reverbWidth", "Width", 0, 1, 1, Glide::Lin, "" },
    { "reverbMix", "Crypt Mix", 0, 1, 0, Glide::Lin, "" },
    { "character", "Chamber", 0, 3, 1, Glide::Step, "Forgotten Chapel|Ancient Cathedral|Underground Crypt|Endless Void" },
    { "cryptMode", "Crypt Mode", 0, 1, 0, Glide::Step, "Normal|Rising" },
    { "riseTime", "Rise Time", .2f, 4, 1.2f, Glide::Log, "" },
    { "eternal", "Eternal", 0, 1, 0, Glide::Step, "Off|On" },

    { "flicker", "Torch Flicker", 0, 1, 0, Glide::Lin, "" },
    { "flickerRate", "Flicker Rate", .3f, 12, 3, Glide::Log, "" },
    { "metal", "Metal", 0, 1, 0, Glide::Lin, "" },
    { "metalKind", "Metal Kind", 0, 2, 0, Glide::Step, "Iron Bars|Bell|Crystal" },
    { "metalNote", "Metal Note", 0, 11, 2, Glide::Step, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
    { "metalOctave", "Metal Octave", 0, 4, 2, Glide::Step, "2|3|4|5|6" },
    { "metalDecay", "Metal Decay", .05f, 8, .8f, Glide::Log, "" },
    { "wind", "Wind", 0, 1, 0, Glide::Lin, "" },
    { "drips", "Drips", 0, 1, 0, Glide::Lin, "" },

    { "sigKind", "Signature", 0, 40, 0, Glide::Step, signatureChoices },
    { "sigAmount", "Signature Amount", 0, 1, 0, Glide::Lin, "" },
    { "sigTone", "Signature Tone", -1, 1, 0, Glide::Lin, "" },
    { "sigRate", "Signature Rate", .25f, 4, 1, Glide::Log, "" },
}};

inline Frame neutralFrame() noexcept {
    Frame f {};
    for (std::size_t i = 0; i < Count; ++i) f[i] = moduleSpecs[i].neutral;
    return f;
}
}
