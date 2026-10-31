#pragma once
// The world: ten locations, each a place of its own (its "scene", sub-location 0) with
// two to four sub-locations, and three or four themed macros. A place is a Frame of
// module settings: the location's base design with the sub-location's changes on top.
// Macros and the Advanced drawer adjust a place; Intensity scales it towards neutral.
// The data is built once at static initialisation and only read afterwards.
#include "DSP/ModuleParams.h"
#include <array>
#include <vector>

namespace dc {
enum Location : int {
    Dungeon, Castle, Cathedral, HauntedForest, Caverns, Village, Swamp, Mountains, Necropolis, WizardsTower,
    LocationCount
};

// Host-visible names, in parameter order. Never reorder: automation stores the index.
inline constexpr std::array<const char*, LocationCount> locationNames {
    "Dungeon", "Castle", "Cathedral", "Haunted Forest", "Caverns",
    "Village", "Swamp", "Mountains", "Necropolis", "Wizard's Tower"
};

inline constexpr int maxSubLocations = 4;   // per location, besides the location's own scene
inline constexpr int placesPerLocation = maxSubLocations + 1;
inline constexpr int macroCount = 4;

struct Change { P id; float value; };
// A macro at 0.5 leaves the place as designed; towards 0 it adds `down`, towards 1 `up`
// (linear in between). For Log parameters the offsets are octaves (factors of 2).
struct MacroTarget { P id; float down, up; };
struct Macro { const char* name; std::vector<MacroTarget> targets; };
struct SubLocation { const char* name; std::vector<Change> changes; };
struct LocationDesign {
    const char* name;
    const char* scene;   // the location's own place (sub-location 0)
    std::vector<Change> base;
    std::vector<SubLocation> subs;
    std::vector<Macro> macros;
};

const LocationDesign& design(int location) noexcept;
int subLocationCount(int location) noexcept;                  // excluding the scene
int clampSubLocation(int location, int sub) noexcept;         // 0 .. subLocationCount
const char* subLocationName(int location, int sub) noexcept;  // 0: the scene's name
int macroCountFor(int location) noexcept;
const char* macroName(int location, int macro) noexcept;      // "" when unused

// The place's signature effect (Signature kind 1..40, unique per place).
int signatureKind(int location, int sub) noexcept;
// The place as designed (macros at 0.5, no Advanced offsets).
Frame placeFrame(int location, int sub) noexcept;
inline Frame locationFrame(int location) noexcept { return placeFrame(location, 0); }
// Macro values 0..1 (0.5 = as designed) applied to a frame, clamped to the ranges.
void applyMacros(Frame&, int location, const std::array<float, macroCount>&) noexcept;
// The calibrated level trim of a place (dB), so the level matcher only fine-tunes.
float placeTrimDb(int location, int sub) noexcept;
}
