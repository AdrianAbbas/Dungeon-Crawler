#pragma once
// DUNGEON CRAWLER's factory presets (Direction Update 3): instrument presets (an
// instrument, a place, its macros) and, after them, the effect's journeys (they move the
// place, and with it the instrument). Like the effect's presets, a preset sets the listed
// parameters on top of the defaults.
#include "../Parameters.h"
#include "../World/Presets.h"
#include "Synth.h"

namespace dc::inst {

inline const std::vector<FactoryPreset>& instrumentPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> p;
        // In its own place: the place chooses the instrument. `m` are the five macros
        // (0.5 = as designed); then Intensity and Atmosphere.
        const auto here = [&p](const char* name, int location, int sub, std::array<float, 5> m = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f },
                               float intensity = 1.0f, float atmosphere = 0.0f) {
            FactoryPreset f { name, "Instruments", { { "location", float(location) }, { "subLocation", float(sub) }, { "intensity", intensity },
                                                     { "atmosLevel", atmosphere } }, {} };
            for (std::size_t k = 0; k < m.size(); ++k) f.values.push_back({ ids::instMacros[k], m[k] });
            p.push_back(std::move(f));
        };
        // Locked: an instrument in another place.
        const auto elsewhere = [&p](const char* name, int fromLocation, int fromSub, int location, int sub, std::array<float, 5> m = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f },
                                    float intensity = 1.0f) {
            FactoryPreset f { name, "Instruments elsewhere", { { "instLock", 1.0f }, { "instrument", float(designFor(fromLocation, fromSub)) },
                                                              { "location", float(location) }, { "subLocation", float(sub) }, { "intensity", intensity } }, {} };
            for (std::size_t k = 0; k < m.size(); ++k) f.values.push_back({ ids::instMacros[k], m[k] });
            p.push_back(std::move(f));
        };
        here("Pizzicato in the Dark", Dungeon, 0, { 0.5f, 0.5f, 0.55f, 0.6f, 0.7f });
        here("Cello at the Gate", Dungeon, 1, { 0.45f, 0.55f, 0.55f, 0.6f, 0.7f });
        here("Chains in the Cells", Dungeon, 2, { 0.5f, 0.6f, 0.5f, 0.5f, 0.7f });
        here("Gold and Glints", Dungeon, 3, { 0.55f, 0.5f, 0.6f, 0.5f, 0.8f });
        here("The Devil's Brass", Dungeon, 4, { 0.42f, 0.5f, 0.55f, 0.65f, 0.8f });
        here("Courtyard Lute", Castle, 0, { 0.5f, 0.5f, 0.5f, 0.55f, 0.6f });
        here("Fanfare for the King", Castle, 1, { 0.55f, 0.45f, 0.5f, 0.5f, 0.8f });
        here("The Royal Court", Castle, 2, { 0.5f, 0.5f, 0.6f, 0.55f, 0.65f });
        here("Vespers", Castle, 3, { 0.45f, 0.5f, 0.5f, 0.5f, 0.75f });
        here("Call from the Tower", Castle, 4, { 0.5f, 0.55f, 0.6f, 0.5f, 0.65f });
        here("Cloister Choir", Cathedral, 0, { 0.45f, 0.6f, 0.6f, 0.5f, 0.3f });
        here("Organ of the Nave", Cathedral, 1, { 0.5f, 0.5f, 0.55f, 0.5f, 0.7f });
        here("Chant below the Crypt", Cathedral, 2, { 0.4f, 0.55f, 0.6f, 0.5f, 0.35f });
        here("Tolling the Hour", Cathedral, 3, { 0.5f, 0.5f, 0.6f, 0.5f, 0.6f }, 1.0f, 0.2f);
        here("Edge of the Wood", HauntedForest, 0, { 0.45f, 0.5f, 0.55f, 0.5f, 0.65f });
        here("Creaking Path", HauntedForest, 1, { 0.45f, 0.55f, 0.55f, 0.5f, 0.75f });
        here("Lullaby for the Lost", HauntedForest, 2, { 0.5f, 0.5f, 0.6f, 0.5f, 0.7f });
        here("Harp of the Grove", HauntedForest, 3, { 0.5f, 0.5f, 0.6f, 0.55f, 0.7f });
        here("Fireside Marimba", Caverns, 0, { 0.5f, 0.45f, 0.55f, 0.6f, 0.6f });
        here("Crystal Bells", Caverns, 1, { 0.55f, 0.5f, 0.6f, 0.5f, 0.7f });
        here("Kalimba by the Lake", Caverns, 2, { 0.5f, 0.5f, 0.55f, 0.5f, 0.6f }, 1.0f, 0.25f);
        here("The Dragon Wakes", Caverns, 3, { 0.45f, 0.5f, 0.6f, 0.6f, 0.8f });
        here("Weary Whistle", Village, 0, { 0.45f, 0.5f, 0.5f, 0.5f, 0.6f });
        here("Last Song at the Tavern", Village, 1, { 0.45f, 0.5f, 0.55f, 0.6f, 0.7f }, 1.0f, 0.3f);
        here("Fiddler in the Square", Village, 2, { 0.5f, 0.5f, 0.5f, 0.55f, 0.55f });
        here("Anvil Song", Village, 3, { 0.5f, 0.55f, 0.55f, 0.5f, 0.6f });
        here("Mire Banjo", Swamp, 0, { 0.5f, 0.5f, 0.5f, 0.6f, 0.65f });
        here("Reed of the Bog", Swamp, 1, { 0.45f, 0.5f, 0.5f, 0.5f, 0.6f });
        here("Bells under Water", Swamp, 2, { 0.5f, 0.5f, 0.6f, 0.5f, 0.7f });
        here("Shepherd of the Hills", Mountains, 0, { 0.5f, 0.5f, 0.55f, 0.5f, 0.45f });
        here("War Drums of the Pass", Mountains, 1, { 0.5f, 0.6f, 0.55f, 0.6f, 0.7f });
        here("Frozen Glockenspiel", Mountains, 2, { 0.55f, 0.5f, 0.55f, 0.5f, 0.6f });
        here("Song of the Dwarves", Mountains, 3, { 0.45f, 0.5f, 0.55f, 0.55f, 0.7f });
        here("Gates of the Dead", Necropolis, 0, { 0.45f, 0.55f, 0.6f, 0.5f, 0.6f });
        here("Ghost Violin", Necropolis, 1, { 0.5f, 0.55f, 0.6f, 0.5f, 0.7f }, 1.0f, 0.2f);
        here("The Mausoleum Bell", Necropolis, 2, { 0.45f, 0.5f, 0.6f, 0.5f, 0.6f });
        here("Spiral Stair", WizardsTower, 0, { 0.5f, 0.5f, 0.5f, 0.5f, 0.4f });
        here("Hush of the Library", WizardsTower, 1, { 0.45f, 0.5f, 0.5f, 0.5f, 0.5f });
        here("Alchemist's Bubbles", WizardsTower, 2, { 0.5f, 0.5f, 0.55f, 0.5f, 0.8f });
        here("Starfield", WizardsTower, 3, { 0.55f, 0.6f, 0.6f, 0.5f, 0.7f });
        elsewhere("Tavern Guitar in the Boss Lair", Village, 1, Dungeon, 4, { 0.5f, 0.5f, 0.55f, 0.55f, 0.5f });
        elsewhere("Church Bells in the Crystal Cavern", Cathedral, 3, Caverns, 1);
        elsewhere("Music Box in the Crypt", HauntedForest, 2, Cathedral, 2, { 0.5f, 0.5f, 0.6f, 0.5f, 0.7f });
        elsewhere("Choir in the Observatory", Cathedral, 0, WizardsTower, 3, { 0.5f, 0.6f, 0.6f, 0.5f, 0.4f });
        elsewhere("Ghost Violin in the Great Hall", Necropolis, 1, Castle, 1);
        elsewhere("Lute by the Lake", Castle, 0, Caverns, 2);
        elsewhere("Dry Harp (no place)", HauntedForest, 3, HauntedForest, 3, { 0.5f, 0.5f, 0.55f, 0.55f, 0.4f }, 0.0f);
        // The effect's journeys: the place moves, and the instrument with it.
        for (const auto& f : factoryPresets())
            if (juce::String(f.category) == "Journeys") p.push_back(f);
        // Session 15: built around DUNGEON SYNTH (after the others, so their indices stay): an
        // instrument (in its place, or locked elsewhere), the amount and the Era (0 Classic,
        // 1 Modern), and sometimes its macros or the place's.
        const auto dungeon = [&p](const char* name, int fromLocation, int fromSub, int location, int sub, float amount, int era,
                                  std::vector<std::pair<const char*, float>> extra = {}) {
            const bool locked = fromLocation != location || fromSub != sub;
            FactoryPreset f { name, "Dungeon synth", { { "location", float(location) }, { "subLocation", float(sub) },
                                                       { ids::dungeonSynth, amount }, { ids::dungeonSynthEra, float(era) } }, {} };
            if (locked) {
                f.values.push_back({ "instLock", 1.0f });
                f.values.push_back({ "instrument", float(designFor(fromLocation, fromSub)) });
            }
            for (const auto& e : extra) f.values.push_back(e);
            p.push_back(std::move(f));
        };
        // Session 16: the stronger stage; the amounts raised to show it off (Classic 90-100 %, Modern 85-100 %).
        dungeon("Cassette Castle Theme", Castle, 2, Castle, 2, 1.0f, 0, { { ids::instMacros[2], 0.6f } });
        dungeon("Modern Dungeon Synth Pad", Necropolis, 0, Necropolis, 0, 1.0f, 1, { { ids::instMacros[1], 0.6f } });
        dungeon("Crypt Organ Drone", Cathedral, 1, Cathedral, 2, 0.9f, 0, { { ids::instMacros[2], 0.65f } });
        dungeon("Lo-fi Fantasy Flute", HauntedForest, 0, HauntedForest, 0, 1.0f, 0);
        dungeon("Tape Harp of the Grove", HauntedForest, 3, HauntedForest, 3, 0.85f, 1);
        dungeon("Frozen Tower on Cassette", Mountains, 2, WizardsTower, 3, 0.9f, 0);
        dungeon("Hall of the Old Kings", Castle, 4, Mountains, 3, 0.9f, 0);
        dungeon("Starlit Modern Pad", WizardsTower, 3, WizardsTower, 3, 1.0f, 1);
        dungeon("Necropolis Choir Tape", Cathedral, 0, Necropolis, 0, 1.0f, 0);
        dungeon("Harpsichord Demo Tape", WizardsTower, 0, WizardsTower, 0, 1.0f, 0, { { "atmosLevel", 0.2f } });
        return p;
    }();
    return presets;
}
}
