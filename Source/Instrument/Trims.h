#pragma once
// Level trims (dB) per instrument, in place order: measured by
// `DungeonCrawlerTests --calibrate-instruments Source/Instrument/Trims.h` (the loudest
// 400 ms of the test melody played dry, K-weighted, brought to the same loudness).
#include <array>

namespace dc::inst {
inline constexpr std::array<float, 40> trims {
    -0.76f,   // Contrabass Pizzicato (measured -20.0 dB)
    -12.68f,   // Cello Drone-Lead (measured -17.2 dB)
    -2.60f,   // Iron Chains (measured -20.0 dB)
    -2.13f,   // Golden Celesta (measured -20.0 dB)
    -13.98f,   // Doom Brass & Choir (measured -19.8 dB)
    8.38f,   // Lute (measured -20.0 dB)
    -15.66f,   // Brass Fanfare (measured -17.0 dB)
    -14.74f,   // Royal Strings (measured -16.5 dB)
    -14.38f,   // Chamber Organ (measured -19.1 dB)
    -12.36f,   // Heroic Horn (measured -19.8 dB)
    3.81f,   // Soft Choir (measured -20.0 dB)
    -11.96f,   // Pipe Organ (measured -19.4 dB)
    2.64f,   // Monk Chant (measured -20.0 dB)
    -12.11f,   // Church Bells (measured -17.8 dB)
    -14.48f,   // Wooden Flute (measured -18.5 dB)
    -8.24f,   // Cello Harmonics (measured -19.6 dB)
    -1.67f,   // Music Box (measured -20.0 dB)
    -0.67f,   // Ancient Harp (measured -20.0 dB)
    -2.95f,   // Stone Marimba (measured -20.0 dB)
    -6.48f,   // Crystal Bells (measured -20.0 dB)
    5.67f,   // Water Kalimba (measured -20.0 dB)
    -11.96f,   // Hoard Gong (measured -19.4 dB)
    -13.12f,   // Tin Whistle (measured -19.9 dB)
    5.76f,   // Weary Guitar (measured -20.0 dB)
    -4.51f,   // Fiddle (measured -20.0 dB)
    -5.13f,   // Tuned Anvil (measured -20.0 dB)
    9.85f,   // Detuned Banjo (measured -20.0 dB)
    -13.24f,   // Murky Reed (measured -20.0 dB)
    -9.40f,   // Drowned Bells (measured -20.0 dB)
    -13.81f,   // Shepherd's Flute (measured -19.1 dB)
    -3.95f,   // War Drums (measured -20.0 dB)
    -3.43f,   // Ice Glockenspiel (measured -20.0 dB)
    -7.55f,   // Dwarven Chant (measured -20.0 dB)
    8.81f,   // Dark Choir Pad (measured -20.0 dB)
    -1.47f,   // Ghost Violin (measured -20.0 dB)
    -14.34f,   // Tolling Bell & Organ (measured -17.4 dB)
    5.22f,   // Harpsichord Arpeggio (measured -20.0 dB)
    7.95f,   // Clavichord (measured -20.0 dB)
    -10.71f,   // Glass Marimba (measured -20.0 dB)
    -1.72f,   // Celestial Pad (measured -20.0 dB)
};
}
