#pragma once
// Level trims (dB) per instrument, in place order: measured by
// `DungeonCrawlerTests --calibrate-instruments Source/Instrument/Trims.h` (the loudest
// 400 ms of the test melody, K-weighted, brought to the same loudness).
#include <array>

namespace dc::inst {
inline constexpr std::array<float, 40> trims {
    -0.76f,   // Contrabass Pizzicato (measured -20.0 dB)
    -12.62f,   // Cello Drone-Lead (measured -20.0 dB)
    -2.60f,   // Iron Chains (measured -20.0 dB)
    -2.13f,   // Golden Celesta (measured -20.0 dB)
    -14.60f,   // Doom Brass & Choir (measured -20.0 dB)
    8.38f,   // Lute (measured -20.0 dB)
    -15.66f,   // Brass Fanfare (measured -20.0 dB)
    -14.69f,   // Royal Strings (measured -20.0 dB)
    -14.38f,   // Chamber Organ (measured -20.0 dB)
    -12.36f,   // Heroic Horn (measured -20.0 dB)
    6.43f,   // Soft Choir (measured -20.0 dB)
    -11.96f,   // Pipe Organ (measured -20.0 dB)
    -5.48f,   // Monk Chant (measured -20.0 dB)
    -12.11f,   // Church Bells (measured -20.0 dB)
    -14.48f,   // Wooden Flute (measured -20.0 dB)
    -8.24f,   // Cello Harmonics (measured -20.0 dB)
    -1.95f,   // Music Box (measured -20.0 dB)
    -0.65f,   // Ancient Harp (measured -20.0 dB)
    -2.95f,   // Stone Marimba (measured -20.0 dB)
    -6.58f,   // Crystal Bells (measured -20.0 dB)
    4.98f,   // Water Kalimba (measured -20.0 dB)
    -11.96f,   // Hoard Gong (measured -20.0 dB)
    -13.12f,   // Tin Whistle (measured -20.0 dB)
    5.76f,   // Weary Guitar (measured -20.0 dB)
    -2.52f,   // Fiddle (measured -20.0 dB)
    -5.13f,   // Tuned Anvil (measured -20.0 dB)
    8.23f,   // Detuned Banjo (measured -20.0 dB)
    -13.24f,   // Murky Reed (measured -20.0 dB)
    -13.46f,   // Drowned Bells (measured -20.0 dB)
    -13.80f,   // Shepherd's Flute (measured -20.0 dB)
    13.99f,   // War Drums (measured -18.7 dB)
    -3.43f,   // Ice Glockenspiel (measured -20.0 dB)
    -8.48f,   // Dwarven Chant (measured -20.0 dB)
    10.20f,   // Dark Choir Pad (measured -20.0 dB)
    -1.47f,   // Ghost Violin (measured -20.0 dB)
    -14.33f,   // Tolling Bell & Organ (measured -20.0 dB)
    5.22f,   // Harpsichord Arpeggio (measured -20.0 dB)
    7.95f,   // Clavichord (measured -20.0 dB)
    -10.71f,   // Glass Marimba (measured -20.0 dB)
    -0.55f,   // Celestial Pad (measured -20.0 dB)
};
// The wet path in each place (instrument only), so the place's instrument is as loud in
// its place as dry.
inline constexpr std::array<float, 40> placeTrims {
    -2.50f,   // Corridors (in place 0.0 dB)
    -5.51f,   // Entrance Hall (in place 0.0 dB)
    -3.55f,   // Prison Cells (in place -0.0 dB)
    -2.69f,   // Treasure Room (in place 0.0 dB)
    -1.94f,   // Final Boss Lair (in place -0.0 dB)
    -11.17f,   // Courtyard (in place 0.0 dB)
    -3.89f,   // Great Hall (in place 0.0 dB)
    -2.68f,   // Throne Room (in place 0.0 dB)
    -7.27f,   // Chapel (in place 0.0 dB)
    -4.69f,   // Tower Top (in place -0.0 dB)
    0.49f,   // Cloister (in place 0.0 dB)
    -5.97f,   // Nave (in place -0.0 dB)
    -6.68f,   // Crypt (in place 0.0 dB)
    -10.40f,   // Bell Tower (in place 0.0 dB)
    -5.07f,   // Forest Edge (in place -0.0 dB)
    -1.29f,   // Old Path (in place 0.0 dB)
    -3.78f,   // Witch's Hut (in place 0.0 dB)
    -4.65f,   // Ancient Grove (in place -0.0 dB)
    -6.79f,   // Cave Mouth (in place 0.0 dB)
    -0.91f,   // Crystal Cavern (in place 0.0 dB)
    1.92f,   // Underground Lake (in place -0.0 dB)
    -5.49f,   // Dragon's Hoard (in place -0.0 dB)
    -3.19f,   // Village Green (in place 0.0 dB)
    -7.50f,   // Tavern (in place 0.0 dB)
    -2.55f,   // Market Square (in place -0.0 dB)
    -8.80f,   // Blacksmith (in place 0.0 dB)
    -2.38f,   // Mire (in place -0.0 dB)
    -5.33f,   // Bog (in place -0.0 dB)
    0.78f,   // Sunken Ruins (in place -0.0 dB)
    -5.04f,   // Foothills (in place 0.0 dB)
    -6.91f,   // Mountain Pass (in place 1.2 dB)
    -3.47f,   // Frozen Peak (in place -0.0 dB)
    -6.51f,   // Dwarven Hall (in place -0.0 dB)
    -0.77f,   // Gates (in place 0.0 dB)
    -1.78f,   // Graveyard (in place 0.0 dB)
    -1.16f,   // Mausoleum (in place -0.0 dB)
    -3.90f,   // Stairwell (in place 0.0 dB)
    -4.44f,   // Library (in place 0.0 dB)
    -2.61f,   // Alchemy Lab (in place 0.0 dB)
    -0.95f,   // Observatory (in place -0.0 dB)
};
// The sampled instruments' synthesised stand-ins (a build without the recordings).
inline constexpr std::array<float, 40> standInTrims {
    -0.76f,   // Contrabass Pizzicato
    -12.62f,   // Cello Drone-Lead
    -2.60f,   // Iron Chains
    -2.13f,   // Golden Celesta
    -14.62f,   // Doom Brass & Choir (stand-in measured -20.0 dB)
    7.68f,   // Lute (stand-in measured -20.0 dB)
    -15.66f,   // Brass Fanfare
    -14.69f,   // Royal Strings
    -14.38f,   // Chamber Organ
    -12.36f,   // Heroic Horn
    3.81f,   // Soft Choir (stand-in measured -20.0 dB)
    -11.96f,   // Pipe Organ
    2.64f,   // Monk Chant (stand-in measured -20.0 dB)
    -12.11f,   // Church Bells
    -14.48f,   // Wooden Flute
    -8.24f,   // Cello Harmonics
    -1.67f,   // Music Box (stand-in measured -20.0 dB)
    -0.67f,   // Ancient Harp (stand-in measured -20.0 dB)
    -2.95f,   // Stone Marimba
    -6.58f,   // Crystal Bells
    -6.46f,   // Water Kalimba (stand-in measured -20.0 dB)
    -11.96f,   // Hoard Gong
    -13.12f,   // Tin Whistle
    2.44f,   // Weary Guitar (stand-in measured -20.0 dB)
    -13.25f,   // Fiddle (stand-in measured -20.0 dB)
    -5.13f,   // Tuned Anvil
    9.85f,   // Detuned Banjo (stand-in measured -20.0 dB)
    -13.24f,   // Murky Reed
    -13.46f,   // Drowned Bells
    -13.80f,   // Shepherd's Flute
    -3.95f,   // War Drums (stand-in measured -20.0 dB)
    -3.43f,   // Ice Glockenspiel
    -7.55f,   // Dwarven Chant (stand-in measured -20.0 dB)
    7.66f,   // Dark Choir Pad (stand-in measured -20.0 dB)
    -11.28f,   // Ghost Violin (stand-in measured -20.0 dB)
    -14.34f,   // Tolling Bell & Organ (stand-in measured -20.0 dB)
    5.22f,   // Harpsichord Arpeggio
    7.95f,   // Clavichord
    -10.71f,   // Glass Marimba
    -9.24f,   // Celestial Pad (stand-in measured -20.0 dB)
};
}
