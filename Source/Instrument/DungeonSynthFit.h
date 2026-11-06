#pragma once
// Session 15: how much of DUNGEON SYNTH's ensemble chorus suits each instrument (in place
// order, as dc::inst::design): the genre's chorus belongs on pads, strings, choirs and
// organs; plucks, bells and mallets take a little; drums and struck iron almost none.
#include <array>
#include <algorithm>

namespace dc::inst {
inline float dungeonSynthChorus(int design) noexcept {
    static constexpr std::array<float, 40> weights {
        0.3f, 1.0f, 0.2f, 0.4f, 0.6f,     // Dungeon: pizzicato, cello, chains, celesta, doom brass
        0.4f, 0.5f, 1.0f, 1.0f, 0.5f,     // Castle: lute, fanfare, strings, chamber organ, horn
        1.0f, 1.0f, 1.0f, 0.4f,           // Cathedral: soft choir, pipe organ, monk chant, bells
        0.7f, 1.0f, 0.5f, 0.5f,           // Haunted Forest: wooden flute, cello harmonics, music box, harp
        0.3f, 0.5f, 0.4f, 0.3f,           // Caverns: stone marimba, crystal bells, kalimba, gong
        0.6f, 0.4f, 1.0f, 0.2f,           // Village: tin whistle, guitar, fiddle, anvil
        0.3f, 0.7f, 0.8f,                 // Swamp: banjo, low reed, underwater bells
        0.7f, 0.1f, 0.4f, 0.9f,           // Mountains: shepherd's flute, war drums, ice glockenspiel, dwarven chant
        1.0f, 1.0f, 0.7f,                 // Necropolis: dark choir pad, ghost violin, bell and organ
        0.4f, 0.4f, 0.4f, 1.0f,           // Wizard's Tower: harpsichord, clavichord, glass marimba, celestial pad
    };
    return weights[static_cast<std::size_t>(std::clamp(design, 0, 39))];
}
// The FX does not know what it is given: a middle amount.
inline constexpr float dungeonSynthChorusFx = 0.6f;
}
