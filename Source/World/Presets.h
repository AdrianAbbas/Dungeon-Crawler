#pragma once
// Factory presets (Milestones 11 and 12): favourite spots and journeys. A preset sets
// the listed parameters on top of the defaults (everything else returns to its
// default) and, for journeys, the route. User presets are saved state files.
#include "Journey.h"
#include <utility>
#include <vector>

namespace dc {
struct FactoryPreset {
    const char* name;
    const char* category;   // "Favourite spots", "Journeys" or (Session 15) "Dungeon synth"
    std::vector<std::pair<const char*, float>> values;   // parameter ID -> plain value
    std::vector<Stop> route;
};

inline const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets {
        // Favourite spots.
        { "Torchlit Corridors", "Favourite spots", { { "location", Dungeon }, { "subLocation", 0 } }, {} },
        { "Glittering Hoard", "Favourite spots", { { "location", Dungeon }, { "subLocation", 3 } }, {} },
        { "Lair of the Beast", "Favourite spots", { { "location", Dungeon }, { "subLocation", 4 }, { "macro4", 0.7f } }, {} },
        { "Endless Nave", "Favourite spots", { { "location", Cathedral }, { "subLocation", 1 }, { "macro1", 0.75f } }, {} },
        { "Bell Tower at Dusk", "Favourite spots", { { "location", Cathedral }, { "subLocation", 3 }, { "atmosLevel", 0.4f } }, {} },
        { "The Witch's Hut", "Favourite spots", { { "location", HauntedForest }, { "subLocation", 2 } }, {} },
        { "Ancient Grove Shimmer", "Favourite spots", { { "location", HauntedForest }, { "subLocation", 3 }, { "macro1", 0.65f } }, {} },
        { "Crystal Cavern", "Favourite spots", { { "location", Caverns }, { "subLocation", 1 } }, {} },
        { "Beneath the Lake", "Favourite spots", { { "location", Caverns }, { "subLocation", 2 }, { "atmosLevel", 0.35f } }, {} },
        { "Warm Tavern Corner", "Favourite spots", { { "location", Village }, { "subLocation", 1 }, { "atmosLevel", 0.45f } }, {} },
        { "Blacksmith's Anvil", "Favourite spots", { { "location", Village }, { "subLocation", 3 } }, {} },
        { "Sunken Ruins", "Favourite spots", { { "location", Swamp }, { "subLocation", 2 } }, {} },
        { "Canyon Echoes", "Favourite spots", { { "location", Mountains }, { "subLocation", 1 } }, {} },
        { "Frozen Summit", "Favourite spots", { { "location", Mountains }, { "subLocation", 2 }, { "atmosLevel", 0.3f } }, {} },
        { "Graveyard Mist", "Favourite spots", { { "location", Necropolis }, { "subLocation", 1 }, { "atmosLevel", 0.5f } }, {} },
        { "Observatory Stars", "Favourite spots", { { "location", WizardsTower }, { "subLocation", 3 } }, {} },
        { "Subtle Chapel (50 %)", "Favourite spots", { { "location", Castle }, { "subLocation", 3 }, { "intensity", 0.5f } }, {} },
        // Journeys.
        { "Descent into the Dungeon", "Journeys",
          { { "journeyOn", 1 }, { "journeyMode", JourneyOneWay }, { "journeyBars", 2 }, { "travelSync", 3 } },
          { { Dungeon, 0 }, { Dungeon, 1 }, { Dungeon, 2 }, { Dungeon, 3 }, { Dungeon, 4 } } },
        { "Grand Castle Tour", "Journeys",
          { { "location", Castle }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 4 }, { "travelSync", 3 } },
          { { Castle, 0 }, { Castle, 1 }, { Castle, 2 }, { Castle, 3 }, { Castle, 4 } } },
        { "Pilgrimage", "Journeys",
          { { "location", Village }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 4 }, { "travelSync", 4 } },
          { { Village, 0 }, { Cathedral, 0 }, { Cathedral, 1 }, { Cathedral, 3 }, { Necropolis, 1 } } },
        { "Into the Haunted Wood", "Journeys",
          { { "location", HauntedForest }, { "journeyOn", 1 }, { "journeyMode", JourneyPingPong }, { "journeyBars", 2 }, { "travelSync", 3 }, { "atmosLevel", 0.3f } },
          { { HauntedForest, 0 }, { HauntedForest, 1 }, { HauntedForest, 2 }, { HauntedForest, 3 } } },
        { "The Dragon's Road", "Journeys",
          { { "location", Mountains }, { "journeyOn", 1 }, { "journeyMode", JourneyOneWay }, { "journeyBars", 4 }, { "travelSync", 4 } },
          { { Mountains, 0 }, { Mountains, 1 }, { Caverns, 0 }, { Caverns, 3 } } },
        { "Wizard's Errand", "Journeys",
          { { "location", WizardsTower }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 2 }, { "travelSync", 2 } },
          { { WizardsTower, 0 }, { WizardsTower, 1 }, { WizardsTower, 2 }, { WizardsTower, 3 } } },
        { "Roaming the Caverns", "Journeys",
          { { "location", Caverns }, { "roamOn", 1 }, { "roamBars", 2 }, { "roamRandomness", 0.5f }, { "travelSync", 3 } }, {} },
        { "Restless Spirits", "Journeys",
          { { "location", Necropolis }, { "roamOn", 1 }, { "roamBars", 1 }, { "roamRandomness", 0.8f }, { "atmosLevel", 0.4f } }, {} },
        { "Swamp Wander", "Journeys",
          { { "location", Swamp }, { "roamOn", 1 }, { "roamBars", 3 }, { "roamRandomness", 0.3f }, { "travelTime", 3.0f } }, {} },
        // Session 15: built around DUNGEON SYNTH (dungeonSynthEra: 0 Classic, 1 Modern).
        { "Cassette Dungeon", "Dungeon synth", { { "location", Dungeon }, { "subLocation", 0 }, { "dungeonSynth", 0.8f }, { "dungeonSynthEra", 0 } }, {} },
        { "Lo-fi Crypt", "Dungeon synth", { { "location", Cathedral }, { "subLocation", 2 }, { "dungeonSynth", 0.65f }, { "dungeonSynthEra", 0 } }, {} },
        { "Modern Dark Hall", "Dungeon synth", { { "location", Necropolis }, { "subLocation", 0 }, { "dungeonSynth", 0.7f }, { "dungeonSynthEra", 1 } }, {} },
    };
    return presets;
}
}
