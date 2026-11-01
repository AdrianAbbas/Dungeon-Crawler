#include "Locations.h"
#include "LevelTrims.h"
#include <algorithm>
#include <cmath>

namespace dc {
namespace {
// Character (Chamber): 0 Forgotten Chapel, 1 Ancient Cathedral, 2 Underground Crypt, 3 Endless Void.
// Interval: 0 octave below, 1 fifth below, 2 fourth above, 3 fifth above, 4 octave above, 5 octave + fifth.
// MetalKind: 0 iron bars, 1 bell, 2 crystal. MetalOctave and TombOctave are indices (0 = lowest).
std::vector<LocationDesign> buildWorld() {
    std::vector<LocationDesign> w;

    // Milestone 2a sound design (Dungeon, Castle, Cathedral, Haunted Forest, Caverns):
    // every place has a signature of its own (see docs/DEVLOG.md, Milestone 2a).
    //--------------------------------------------------------------------------
    w.push_back({ "Dungeon", "Corridors",
        // Long stone corridors: a dark, damp, mid-sized stone room, a short slap down
        // the passage, torches flickering, water dripping somewhere.
        { { Cutoff, 4500 }, { Darkness, .4f }, { HighPass, 55 }, { Resonance, .1f },
          { Drive, 3 }, { BloodMix, .2f }, { Age, .08f },
          { Flicker, .2f }, { FlickerRate, 3.5f }, { Drips, .22f },
          { DelayTime, 140 }, { Feedback, .2f }, { DelayDamping, .75f }, { Diffusion, .75f }, { Spread, .5f }, { DelayMix, .12f },
          { Character, 2 }, { Space, .55f }, { Decay, 2.5f }, { Damping, .62f }, { PreDelay, 14 }, { ReverbMix, .4f } },
        {
          // A vaulted entrance (CHANGE, strengthened): a big stone vault (Vaulted Hall),
          // the torches brighter, a wide slap from the far door, the stone flutter.
          { "Entrance Hall", { { Cutoff, 6000 }, { Darkness, .3f }, { HighPass, 45 }, { Flicker, .5f }, { FlickerRate, 2.6f }, { Drips, .06f },
                               { DelayTime, 310 }, { Feedback, .28f }, { Diffusion, .3f }, { DelayDamping, .6f }, { Spread, .9f }, { DelayMix, .2f },
                               { Character, 2 }, { Space, .9f }, { Decay, 3.5f }, { Damping, .55f }, { PreDelay, 38 }, { ReverbMix, .3f },
                               { VaultKind, 13 }, { VaultMix, .45f } } },
          // A cramped cell (LIKE, kept): boxy and muffled, iron bars ringing, a flutter
          // between the walls, grime on everything. The old water drops sounded
          // unrealistic: replaced by rare, very quiet modelled drops (Ensemble) in the
          // cell's stone.
          { "Prison Cells", { { Cutoff, 1900 }, { Resonance, .25f }, { Darkness, .45f }, { HighPass, 130 },
                              { Drive, 5 }, { Age, .35f }, { Dust, .12f }, { BloodMix, .35f },
                              { Metal, .42f }, { MetalKind, 0 }, { MetalNote, 2 }, { MetalOctave, 2 }, { MetalDecay, .7f }, { Drips, 0 }, { Drops, .45f }, { Flicker, .12f },
                              { DelayTime, 55 }, { Feedback, .42f }, { Diffusion, .15f }, { DelayDamping, .5f }, { Spread, .3f }, { DelayMix, .16f },
                              { Character, 0 }, { Space, .08f }, { Decay, .7f }, { Damping, .7f }, { PreDelay, 2 }, { ReverbMix, .38f } } },
          // Gold everywhere (CHANGE): rich, like sound reflecting off coins. A bright
          // golden space full of glints (Golden Vault), a lush three-voice ensemble, an
          // octave and twelfth glittering on every attack (signature), a metallic sheen.
          { "Treasure Room", { { Cutoff, 20000 }, { Darkness, 0 }, { HighPass, 110 }, { Resonance, 0 }, { BloodMix, .3f }, { Drive, 7 }, { Texture, .2f }, { Age, 0 },
                               { Metal, .45f }, { MetalKind, 2 }, { MetalNote, 9 }, { MetalOctave, 4 }, { MetalDecay, 1.4f }, { Flicker, .1f }, { Drips, 0 },
                               { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .22f }, { Formant, .85f }, { ChoirDetune, 9 },
                               { EnsMix, .85f }, { EnsDepth, 5 }, { EnsRate, .55f },
                               { DelayTime, 250 }, { DelaySync, 2 }, { Feedback, .38f }, { Spread, 1 }, { DelayDamping, .12f }, { Diffusion, .2f }, { DelayMix, .16f },
                               { Character, 0 }, { Space, .55f }, { Decay, 3 }, { Damping, .12f }, { PreDelay, 18 }, { Shimmer, .45f }, { ReverbMix, .3f },
                               { VaultKind, 14 }, { VaultMix, .76f }, { VaultTone, 9000 } } },
          // The lair (CHANGE): huge and sinister, the devil himself. A colossal dark
          // space (Infernal Abyss, 7.5 s), a sub-octave choir, menacing drive, slow
          // ominous delays that fall a fourth with every repeat (a pitch-down shadow),
          // echoes sinking a semitone, a low resonance.
          { "Final Boss Lair", { { Cutoff, 3200 }, { Darkness, .55f }, { HighPass, 30 }, { Drive, 18 }, { BloodMix, .55f }, { Texture, .6f },
                                 { Voices, 3 }, { Interval, 0 }, { ChoirBlend, .3f }, { Formant, .2f }, { ChoirDetune, 22 }, { Drone, .22f }, { DroneHold, 2.5f },
                                 { Tomb, .25f }, { TombNote, 2 }, { TombOctave, 1 }, { Flicker, .3f }, { FlickerRate, 1.6f }, { Drips, 0 },
                                 { WraithMix, .1f }, { Blur, .5f },
                                 { TapeMix, .28f }, { TapeTime, 820 }, { TapeFeedback, .3f }, { TapeTone, 1800 }, { TapeWow, .3f }, { TapeAscend, 4 },
                                 { DelayTime, 760 }, { Feedback, .45f }, { DelayDamping, .65f }, { Descent, 1 }, { Diffusion, .5f }, { DelayMix, .16f },
                                 { Character, 3 }, { Space, .95f }, { Decay, 6 }, { Damping, .55f }, { PreDelay, 60 }, { ReverbMix, .18f },
                                 { VaultKind, 15 }, { VaultMix, .66f } } },
        },
        {
          { "Depth", { { Space, -.35f, .35f }, { Decay, -1, 1 }, { PreDelay, -12, 30 }, { ReverbMix, -.15f, .15f } } },
          { "Damp", { { Damping, -.3f, .3f }, { Cutoff, 1.5f, -1.5f }, { Darkness, -.2f, .3f }, { Drips, -.25f, .4f } } },
          { "Echo", { { DelayMix, -.12f, .2f }, { Feedback, -.2f, .25f }, { DelayTime, -.5f, .6f } } },
          { "Dread", { { Drive, -4, 12 }, { BloodMix, -.2f, .3f }, { Drone, 0, .35f }, { Tomb, 0, .3f }, { Cutoff, 1, -.7f }, { Darkness, -.3f, .2f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Castle", "Courtyard",
        // Direction Update 2. An open courtyard between high walls, fluttery and
        // mystical: an open stone court (Stone Court) with fluttering reflections, the
        // walls' ping-pong scattering, a gentle shimmer and slow modulation, a breeze.
        { { HighPass, 110 }, { Cutoff, 9000 }, { Darkness, .05f }, { Wind, .18f },
          { EnsMix, .42f }, { EnsDepth, 3.5f }, { EnsRate, .25f }, { WraithMix, .16f }, { Blur, .5f }, { Ghost, .25f },
          { DelayTime, 210 }, { Feedback, .3f }, { Diffusion, .15f }, { DelayDamping, .3f }, { Spread, 1 }, { DelayMix, .3f },
          { Character, 0 }, { Space, .6f }, { Decay, 1.5f }, { Damping, .35f }, { PreDelay, 22 }, { ReverbMix, .16f },
          { VaultKind, 16 }, { VaultMix, .74f }, { VaultShimmer, .25f } },
        {
          // The great hall: MASSIVE. A huge, long, wide and grand hall (Great Hall,
          // 5.5 s, slow bloom), timber warmth, a fire's flicker, banner reflections.
          { "Great Hall", { { HighPass, 35 }, { Cutoff, 8500 }, { Darkness, .12f }, { Drive, 3 }, { Texture, .2f }, { BloodMix, .15f },
                            { Flicker, .1f }, { FlickerRate, 2.2f }, { Wind, 0 }, { EnsMix, 0 }, { WraithMix, 0 },
                            { DelayMix, 0 },
                            { Character, 1 }, { Space, .9f }, { Decay, 4 }, { Damping, .55f }, { PreDelay, 30 }, { ReverbMix, .25f },
                            { VaultKind, 17 }, { VaultMix, .8f }, { VaultShimmer, 0 } } },
          // The throne room: rich. A regal, warm hall (Regal Hall), a lush ensemble, a
          // slight harmonic richness (warm drive), the organ-like fifth above, the
          // brass resonance (signature).
          { "Throne Room", { { HighPass, 40 }, { Cutoff, 9000 }, { Wind, 0 }, { WraithMix, 0 }, { Drive, 5 }, { Texture, .3f }, { BloodMix, .28f },
                             { Voices, 2 }, { Interval, 3 }, { ChoirBlend, .14f }, { Formant, .5f }, { ChoirDetune, 8 }, { ChoirMovement, .2f },
                             { EnsMix, .6f }, { EnsDepth, 4 }, { EnsRate, .35f },
                             { DelayTime, 260 }, { Feedback, .18f }, { Diffusion, .6f }, { DelayMix, .07f },
                             { Character, 1 }, { Space, .95f }, { Decay, 4.2f }, { Damping, .3f }, { PreDelay, 50 }, { Shimmer, .14f }, { ReverbWidth, 1 }, { ReverbMix, .2f },
                             { VaultKind, 18 }, { VaultMix, .62f }, { VaultShimmer, 0 } } },
          // The chapel: quieter, intimate, a sermon in progress. Softer, warmer, close
          // (Small Chapel), slightly muffled, a gentle choir blooming under the
          // instrument (signature) and a soft choir an octave below.
          { "Chapel", { { Cutoff, 3400 }, { Darkness, .32f }, { HighPass, 60 }, { Wind, 0 }, { EnsMix, 0 }, { WraithMix, 0 },
                        { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .18f }, { Formant, .55f }, { ChoirMovement, .4f }, { Drone, .08f }, { DroneHold, 3 },
                        { DelayMix, 0 },
                        { Character, 0 }, { Space, .4f }, { Decay, 2.5f }, { Damping, .7f }, { PreDelay, 12 }, { Shimmer, 0 }, { ReverbMix, .2f },
                        { VaultKind, 19 }, { VaultMix, .6f }, { VaultShimmer, 0 } } },
          // On the battlements: open, looking out over the world ahead. A wide, airy
          // open sky (almost no reflections), long distant echoes, bright, the wind.
          { "Tower Top", { { HighPass, 200 }, { Cutoff, 15000 }, { Darkness, 0 }, { Wind, .28f }, { EnsMix, 0 },
                           { WraithMix, .1f }, { Blur, .6f },
                           { DelayTime, 700 }, { Feedback, .42f }, { DelayDamping, .5f }, { Diffusion, .3f }, { Spread, 1 }, { DelayMix, .38f },
                           { Character, 3 }, { Space, .35f }, { Decay, 2 }, { Damping, .5f }, { PreDelay, 100 }, { ReverbLowCut, 320 }, { ReverbMix, .08f },
                           { VaultKind, 20 }, { VaultMix, .7f }, { VaultShimmer, 0 } } },
        },
        {
          { "Hall Size", { { Space, -.35f, .3f }, { Decay, -1, 1 }, { PreDelay, -15, 30 }, { VaultMix, -.3f, .15f } } },
          { "Grandeur", { { Shimmer, -.1f, .3f }, { ChoirBlend, -.1f, .2f }, { ReverbWidth, -.4f, 0 }, { Drone, 0, .15f }, { EnsMix, -.2f, .2f } } },
          { "Distance", { { ReverbMix, -.2f, .25f }, { Cutoff, 1, -1.5f }, { DelayMix, -.08f, .12f }, { HighPass, -.5f, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Cathedral", "Cloister",
        // Arcades around a garden: a flutter of reflections between the arches,
        // feeding a long stone reverb. LIKE (Direction Update 2): boosted overall, the
        // same character, a bright stone arcade (Old Cloister) behind it.
        { { Cutoff, 9000 }, { Darkness, .12f }, { Wind, .1f },
          { DelayTime, 92 }, { Feedback, .55f }, { Diffusion, .7f }, { DelayDamping, .4f }, { Spread, .6f }, { DelayMix, .34f },
          { Character, 1 }, { Space, .6f }, { Decay, 4.5f }, { Damping, .5f }, { PreDelay, 28 }, { Shimmer, .14f }, { ReverbMix, .5f },
          { VaultKind, 27 }, { VaultMix, .5f } },
        {
          // The nave (CHANGE): the longest, most majestic space in the plug-in. Huge,
          // smooth stone, a long pre-delay and a slow bloom (Grand Nave, 7.5 s), the
          // vault's cascades (signature), a breath of choir.
          { "Nave", { { Cutoff, 6500 }, { Darkness, .15f }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .08f }, { Formant, .55f }, { DelayMix, 0 },
                      { Character, 1 }, { Space, 1 }, { Decay, 9 }, { Damping, .45f }, { PreDelay, 80 }, { Shimmer, .05f }, { ReverbMix, .22f },
                      { VaultKind, 8 }, { VaultMix, .76f }, { VaultDuck, .1f } } },
          // Below the floor (LIKE the filters, kept): very dark and close, a low
          // resonance, a murmur an octave down. New: notes drawn in backwards, a dark
          // reversed swell (Crypt Swell) rising out of the stone.
          { "Crypt", { { Cutoff, 1500 }, { Darkness, .62f }, { HighPass, 28 }, { Wind, 0 }, { Drips, .14f },
                       { Tomb, .32f }, { TombNote, 2 }, { TombOctave, 1 }, { Drive, 3 }, { Age, .15f }, { BloodMix, .2f },
                       { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .15f }, { Formant, .1f }, { DelayMix, 0 },
                       { Character, 2 }, { Space, .3f }, { Decay, 2 }, { Damping, .85f }, { PreDelay, 6 }, { Shimmer, 0 }, { ReverbMix, .45f },
                       { VaultKind, 7 }, { VaultMix, .3f } } },
          // The belfry (CHANGE): you strike a bell. Tuned bell partials (hum, prime,
          // tierce, quint, nominal) ring with every attack (signature and the bell
          // resonators), the ringing fed back through a tape loop, a slow metallic swirl
          // (rising phaser), a hard bright stone tower.
          { "Bell Tower", { { HighPass, 150 }, { Cutoff, 5500 }, { Darkness, .08f }, { Wind, .18f },
                            { Metal, .9f }, { MetalKind, 1 }, { MetalNote, 2 }, { MetalOctave, 1 }, { MetalDecay, 6 },
                            { Spiral, .5f }, { SpiralRate, .05f },
                            { TapeMix, .42f }, { TapeTime, 420 }, { TapeFeedback, .6f }, { TapeTone, 5500 }, { TapeWow, .1f },
                            { DelayMix, 0 },
                            { Character, 0 }, { Space, .45f }, { Decay, 2.1f }, { Damping, .35f }, { PreDelay, 40 }, { Shimmer, .08f }, { ReverbMix, .2f },
                            { VaultKind, 9 }, { VaultMix, .7f } } },
        },
        {
          { "Vastness", { { Space, -.4f, 0 }, { Decay, -1.3f, .8f }, { PreDelay, -20, 40 }, { ReverbMix, -.15f, .12f }, { VaultMix, -.3f, .15f } } },
          { "Choir", { { ChoirBlend, -.15f, .25f }, { Drone, 0, .2f }, { Voices, 0, 1 } } },
          { "Bells", { { Metal, -.3f, .25f }, { MetalDecay, -1, 1 }, { Shimmer, -.08f, .2f }, { Spiral, -.2f, .3f } } },
          { "Gloom", { { Cutoff, 1, -2 }, { Darkness, -.15f, .4f }, { Damping, -.2f, .3f }, { VaultTone, 0, -2 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Haunted Forest", "Forest Edge",
        // Open air under the first trees: soft, damped, airy, wind in the leaves, a
        // faint haze, diffuse echoes from the trunks.
        { { Cutoff, 9000 }, { Darkness, .15f }, { HighPass, 40 }, { Wind, .36f },
          { WraithMix, .12f }, { Blur, .5f }, { Ghost, .15f },
          { DelayTime, 380 }, { Feedback, .25f }, { DelayDamping, .7f }, { Diffusion, .85f }, { Spread, .8f }, { DelayMix, .16f },
          { Character, 0 }, { Space, .4f }, { Decay, 1.4f }, { Damping, .8f }, { ReverbMix, .15f } },
        {
          // The old path: spectral ghosts of every note, whispers played backwards.
          { "Old Path", { { Cutoff, 5000 }, { Darkness, .4f }, { FilterMovement, .25f }, { FilterRate, .07f }, { Wind, .2f },
                          { WraithMix, .45f }, { Blur, .75f }, { Ghost, .55f }, { WraithDetune, 30 }, { Drift, .6f },
                          { DelayTime, 560 }, { Feedback, .35f }, { DelayDamping, .75f }, { Diffusion, .7f }, { Reverse, 1 }, { DelayMix, .2f },
                          { Space, .5f }, { Decay, 2 }, { Damping, .75f }, { PreDelay, 20 }, { ReverbMix, .2f } } },
          // A small wooden hut: boxy, woody, a candle, a crooked choir a fifth below.
          { "Witch's Hut", { { Cutoff, 8000 }, { Darkness, .2f }, { HighPass, 70 }, { Wind, 0 },
                             { Drive, 5 }, { BloodMix, .3f }, { Texture, .45f }, { Age, .25f }, { Instability, .2f }, { Flicker, .3f }, { FlickerRate, 4.5f },
                             { Tomb, .14f }, { TombNote, 9 }, { TombOctave, 2 },
                             { Voices, 1 }, { Interval, 1 }, { ChoirBlend, .12f }, { ChoirDetune, 30 }, { ChoirMovement, .7f }, { Formant, .3f },
                             { WraithMix, 0 },
                             { DelayTime, 48 }, { Feedback, .25f }, { Diffusion, .3f }, { DelayDamping, .6f }, { DelayMix, .1f },
                             { Space, .08f }, { Decay, .5f }, { Damping, .55f }, { PreDelay, 2 }, { ReverbMix, .35f } } },
          // The grove: mystical; a vast soft halo, an octave of air above, slow blur.
          { "Ancient Grove", { { Cutoff, 15000 }, { Darkness, 0 }, { FilterMovement, .2f }, { FilterRate, .05f }, { Wind, .2f },
                               { WraithMix, .3f }, { Blur, .85f }, { Ghost, .25f }, { Drift, .3f },
                               { Voices, 2 }, { Interval, 5 }, { ChoirBlend, .14f }, { Formant, .7f }, { ChoirMovement, .6f },
                               { DelayMix, .08f },
                               { Character, 3 }, { Space, .65f }, { Decay, 4.5f }, { Damping, .45f }, { PreDelay, 40 }, { Shimmer, .35f }, { ReverbMix, .42f } } },
        },
        {
          { "Mist", { { WraithMix, -.12f, .3f }, { Blur, -.3f, .2f }, { Drift, -.2f, .3f } } },
          { "Whispers", { { Ghost, -.2f, .4f }, { DelayMix, -.1f, .15f }, { Feedback, -.15f, .2f } } },
          { "Wind", { { Wind, -.3f, .4f }, { FilterMovement, -.15f, .3f }, { HighPass, 0, 1.5f } } },
          { "Shadows", { { Cutoff, 1, -1.5f }, { Darkness, -.1f, .35f }, { Decay, -.5f, .8f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Caverns", "Cave Mouth",
        // Direction Update 2. The mouth of the cave, enclosed: rocky early reflections
        // close around you (Rock Mouth), a medium cave tail, warm; firelight flickers on
        // the reflections and the cave's hum (the signature's layers), a far answer
        // from deep inside.
        { { HighPass, 60 }, { Cutoff, 6500 }, { Darkness, .22f }, { Drive, 3 }, { BloodMix, .15f }, { Texture, .3f },
          { Drips, .2f }, { Wind, .06f }, { Flicker, .2f }, { FlickerRate, 2.8f },
          { DelayTime, 130 }, { Feedback, .32f }, { Diffusion, .5f }, { DelayDamping, .55f }, { Spread, .85f }, { DelayMix, .16f },
          { Character, 2 }, { Space, .5f }, { Decay, 2.2f }, { Damping, .45f }, { PreDelay, 12 }, { ReverbMix, .22f },
          { VaultKind, 10 }, { VaultMix, .62f } },
        {
          // Crystal (CHANGE): shimmering and magical, a room full of crystals. An octave
          // shimmer climbing in a bright Crystal Hall, crystal resonators ringing, struck
          // glass modes (signature), glassy sparkle.
          { "Crystal Cavern", { { Cutoff, 20000 }, { HighPass, 280 }, { Darkness, 0 }, { Wind, 0 }, { Drive, 0 }, { BloodMix, 0 }, { Flicker, 0 },
                                { Metal, .8f }, { MetalKind, 2 }, { MetalNote, 4 }, { MetalOctave, 3 }, { MetalDecay, 4.5f }, { Drips, .4f },
                                { DelayTime, 230 }, { Feedback, .45f }, { DelayDamping, .08f }, { Diffusion, .1f }, { Spread, 1 }, { DelayMix, .14f },
                                { Character, 1 }, { Space, .6f }, { Decay, 5 }, { Damping, .2f }, { PreDelay, 30 }, { Shimmer, .2f }, { ReverbMix, .3f },
                                { VaultKind, 11 }, { VaultMix, .8f }, { VaultShimmer, .5f }, { VaultTone, 5500 } } },
          // A still lake under the rock (LIKE, near perfect: kept exactly). The Cave
          // Mouth's new settings are pinned back to before.
          { "Underground Lake", { { Cutoff, 2200 }, { Resonance, .35f }, { FilterMovement, .45f }, { FilterRate, .12f }, { Darkness, .3f }, { Wind, 0 },
                                  { Instability, .45f }, { BloodMix, .25f }, { Drips, .6f },
                                  { WraithMix, .2f }, { Blur, .6f }, { Drift, .6f },
                                  { DelayTime, 650 }, { Feedback, .55f }, { DelayDamping, .8f }, { Diffusion, .85f }, { DelayMix, .25f },
                                  { Character, 3 }, { Space, .8f }, { Decay, 8 }, { Damping, .75f }, { PreDelay, 80 }, { ReverbMix, .48f },
                                  { HighPass, 60 }, { Spread, .85f }, { Drive, 0 }, { Texture, .35f }, { Flicker, 0 }, { FlickerRate, 3 },
                                  { VaultKind, 0 }, { VaultMix, 0 } } },
          // The hoard (CHANGE): the vast cavern inside the mountain where the dragon
          // sleeps. Enormous and dry-dark (Vast Cavern, 6.5 s): long distinct far echoes
          // of anything that falls, a metallic sheen of coins on the reflections, a deep
          // rumble (sub-octave voice, drone, low resonance, growl), a slow heavy
          // movement. No watery filter, no wobble: vast, not wet.
          { "Dragon's Hoard", { { Cutoff, 4800 }, { Darkness, .35f }, { HighPass, 25 }, { Wind, 0 }, { Drips, 0 }, { Flicker, 0 },
                                { Drive, 8 }, { BloodMix, .3f }, { Texture, .5f },
                                { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .16f }, { Formant, .12f }, { Drone, .2f }, { DroneHold, 10 },
                                { Tomb, .28f }, { TombNote, 2 }, { TombOctave, 0 },
                                { Metal, .16f }, { MetalKind, 2 }, { MetalNote, 2 }, { MetalOctave, 4 }, { MetalDecay, 1.5f },
                                { EnsMix, .3f }, { EnsDepth, 7 }, { EnsRate, .05f },
                                { DelayMix, 0 },
                                { Character, 1 }, { Space, 1 }, { Decay, 7 }, { Damping, .7f }, { PreDelay, 70 }, { ReverbMix, .18f },
                                { VaultKind, 12 }, { VaultMix, .72f } } },
        },
        {
          { "Depth", { { Space, -.35f, .15f }, { Decay, -1, 1 }, { PreDelay, -25, 40 }, { VaultMix, -.25f, .15f } } },
          { "Drips", { { Drips, -.35f, .45f } } },
          { "Echo", { { DelayMix, -.15f, .2f }, { Feedback, -.25f, .25f }, { DelayTime, -.6f, .6f } } },
          { "Rumble", { { Drone, 0, .35f }, { Drive, -4, 10 }, { BloodMix, -.15f, .25f }, { Tomb, 0, .25f } } },
        } });

    // Milestone 6 sound design (Village, Swamp, Mountains, Necropolis, Wizard's Tower).
    // Each place's signature (Signature.cpp) sits on top of the room designed here.
    //--------------------------------------------------------------------------
    w.push_back({ "Village", "Village Green",
        // Direction Update 2: the whole village is weary, we are in the dark realm. The
        // green: a calming, slow modulation that softens everything (a gentle ensemble,
        // the Folk Chorus doubling), softened highs, soft open air between cottages.
        { { HighPass, 45 }, { Cutoff, 5500 }, { Darkness, .22f }, { Wind, .08f },
          { EnsMix, .55f }, { EnsDepth, 4.5f }, { EnsRate, .18f },
          { DelayTime, 160 }, { Feedback, .12f }, { Diffusion, .5f }, { DelayDamping, .6f }, { Spread, .9f }, { DelayMix, .1f },
          { Character, 0 }, { Space, .4f }, { Decay, 1.1f }, { Damping, .55f }, { PreDelay, 12 }, { ReverbMix, .14f },
          { VaultKind, 33 }, { VaultMix, .5f } },
        {
          // The tavern: calm, almost empty, a few weary travellers. A small warm wooden
          // room (Tavern Room), soft and a little dull, gentle saturation, the fire.
          { "Tavern", { { Cutoff, 4200 }, { Darkness, .32f }, { HighPass, 60 }, { Wind, 0 }, { Flicker, .12f }, { FlickerRate, 2.4f },
                        { Drive, 5 }, { Texture, .3f }, { BloodMix, .3f }, { EnsMix, 0 },
                        { Tomb, .1f }, { TombNote, 7 }, { TombOctave, 1 }, { DelayMix, 0 },
                        { Character, 0 }, { Space, .18f }, { Decay, .75f }, { Damping, .72f }, { PreDelay, 4 }, { ReverbMix, .2f },
                        { VaultKind, 31 }, { VaultMix, .6f } } },
          // The market square: slow, dead, darker. Dark and damped (Dead Square),
          // sparse slaps off the empty stalls (signature, darkened), a slowed, dragging
          // modulation.
          { "Market Square", { { HighPass, 70 }, { Cutoff, 3200 }, { Darkness, .38f }, { Wind, .1f }, { DelayMix, 0 },
                               { EnsMix, .45f }, { EnsDepth, 9 }, { EnsRate, .07f },
                               { Character, 1 }, { Space, .55f }, { Decay, 1.5f }, { Damping, .7f }, { PreDelay, 24 }, { ReverbWidth, 1 }, { ReverbMix, .14f },
                               { VaultKind, 29 }, { VaultMix, .55f } } },
          // The smithy: the only loud space in the village. Anvils ringing (signature,
          // louder) and hot iron resonating, hard bright metallic reflections (Forge
          // Room), gritty drive, the forge flickering fast.
          { "Blacksmith", { { Cutoff, 13000 }, { Darkness, 0 }, { HighPass, 90 }, { Wind, 0 }, { Drive, 14 }, { BloodMix, .5f }, { Texture, .75f }, { Dust, .25f },
                            { Metal, .75f }, { MetalKind, 0 }, { MetalNote, 5 }, { MetalOctave, 3 }, { MetalDecay, 1.6f },
                            { Flicker, .4f }, { FlickerRate, 6.5f }, { DelayMix, 0 }, { EnsMix, 0 },
                            { Character, 2 }, { Space, .28f }, { Decay, 1.1f }, { Damping, .3f }, { PreDelay, 6 }, { ReverbMix, .18f },
                            { VaultKind, 30 }, { VaultMix, .66f } } },
        },
        {
          { "Warmth", { { Drive, -3, 6 }, { Texture, -.15f, .3f }, { Cutoff, 1, -1 }, { Damping, -.2f, .25f } } },
          { "Bustle", { { DelayMix, -.08f, .15f }, { Space, -.15f, .25f }, { ReverbMix, -.1f, .15f }, { SigAmount, -.4f, .3f }, { VaultMix, -.2f, .2f } } },
          { "Grit", { { Dust, 0, .4f }, { Age, 0, .4f }, { BloodMix, 0, .25f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Swamp", "Mire",
        // The mire: thick, damp air over mud; dull and slow, attacks dragged (Mud Drag),
        // slow murky filter movement, drips.
        { { Cutoff, 4500 }, { Resonance, .12f }, { Darkness, .15f }, { FilterMovement, .2f }, { FilterRate, .15f },
          { Instability, .25f }, { BloodMix, .2f }, { Drips, .28f },
          { DelayTime, 380 }, { Feedback, .3f }, { DelayDamping, .78f }, { Diffusion, .6f }, { DelayMix, .14f },
          { Character, 2 }, { Space, .45f }, { Decay, 2 }, { Damping, .78f }, { PreDelay, 18 }, { ReverbMix, .22f } },
        {
          // The bog: deep and thick, everything low-passed and wobbling, bubbles of drips.
          { "Bog", { { Cutoff, 2600 }, { Resonance, .3f }, { Slope, 1 }, { Darkness, .3f }, { FilterMovement, .35f }, { FilterRate, .3f },
                     { Drive, 4 }, { Instability, .45f }, { Age, .22f }, { BloodMix, .35f }, { Drips, .4f },
                     { DelayTime, 430 }, { Feedback, .36f }, { DelayDamping, .82f }, { DelayMix, .16f },
                     { Space, .5f }, { Decay, 2.4f }, { Damping, .82f }, { ReverbMix, .24f } } },
          // Sunken ruins: half under water, echoes off drowned stone, a slow haze.
          { "Sunken Ruins", { { Cutoff, 2800 }, { Resonance, .25f }, { Slope, 1 }, { FilterMovement, .2f }, { Instability, .1f }, { Drips, .3f },
                              { WraithMix, .18f }, { Blur, .7f },
                              { DelayTime, 540 }, { Feedback, .5f }, { DelayDamping, .88f }, { Diffusion, .85f }, { DelayMix, .26f },
                              { Character, 3 }, { Space, .7f }, { Decay, 3.5f }, { Damping, .88f }, { PreDelay, 35 }, { ReverbMix, .38f } } },
        },
        {
          { "Murk", { { Cutoff, 1.5f, -1.5f }, { Resonance, -.15f, .2f }, { Darkness, -.2f, .3f } } },
          { "Wobble", { { Instability, -.25f, .4f }, { FilterMovement, -.2f, .4f }, { SigRate, -1, 1 } } },
          { "Damp", { { Damping, -.3f, .15f }, { DelayMix, -.12f, .15f }, { Drips, -.2f, .4f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Mountains", "Foothills",
        // Direction Update 2. Open hills: sound travels from hill to hill. Echoes
        // scattered across the stereo field and fluttering (the Valley Answer signature,
        // now a fan of answers), a light tape wow on the repeats, which grow more
        // distant (darker) each time, an open-air space, the wind.
        { { HighPass, 110 }, { Cutoff, 7000 }, { Darkness, .1f }, { Wind, .22f }, { DelayMix, 0 },
          { TapeMix, .48f }, { TapeTime, 455 }, { TapeFeedback, .38f }, { TapeTone, 3600 }, { TapeWow, .55f },
          { Character, 0 }, { Space, .3f }, { Decay, .9f }, { Damping, .5f }, { PreDelay, 10 }, { ReverbMix, .1f },
          { VaultKind, 3 }, { VaultMix, .62f } },
        {
          // The pass: massive. A big canyon of a space, and a tape echo bouncing between
          // the left and right walls, darker with every repeat, high but controlled
          // feedback; the canyon echoes (signature) recede behind it.
          { "Mountain Pass", { { HighPass, 90 }, { Cutoff, 20000 }, { Darkness, 0 }, { Wind, .14f },
                               { TapeMix, .8f }, { TapeTime, 330 }, { TapeFeedback, .66f }, { TapeTone, 3000 }, { TapeWow, .22f },
                               { Character, 3 }, { Space, .6f }, { Decay, 2.6f }, { Damping, .25f }, { PreDelay, 70 }, { ReverbLowCut, 220 }, { ReverbMix, .12f },
                               { VaultKind, 4 }, { VaultMix, .82f } } },
          // The peak: sharp, frozen, jagged. Almost no lows left (a steep low cut),
          // crystal modes ringing, glassy short reflections (Ice Shards), an icy shimmer
          // two octaves up (signature) and in the space.
          { "Frozen Peak", { { HighPass, 330 }, { Slope, 1 }, { Cutoff, 9000 }, { Darkness, 0 }, { Wind, .3f },
                             { Metal, .65f }, { MetalKind, 2 }, { MetalNote, 11 }, { MetalOctave, 2 }, { MetalDecay, 3 },
                             { WraithMix, .14f }, { Blur, .4f }, { Ghost, 0 },
                             { TapeMix, 0 },
                             { Character, 3 }, { Space, .45f }, { Decay, 2.6f }, { Damping, .08f }, { PreDelay, 90 }, { Shimmer, .1f }, { ReverbLowCut, 400 }, { ReverbMix, .16f },
                             { VaultKind, 5 }, { VaultMix, .72f }, { VaultShimmer, .1f }, { VaultTone, 5000 } } },
          // The dwarven hall (LIKE the filter, kept): deep stone carved into the
          // mountain, warm with the forge. New: the hall itself, a big deep stone space
          // with a resonant low-mid body (Stone Hall, a low D stone resonance).
          { "Dwarven Hall", { { HighPass, 28 }, { Cutoff, 6500 }, { Darkness, .28f }, { Wind, 0 }, { Drive, 4 }, { BloodMix, .18f }, { Flicker, .22f }, { FlickerRate, 2 },
                              { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .1f }, { Formant, .2f },
                              { Tomb, .22f }, { TombNote, 2 }, { TombOctave, 1 },
                              { TapeMix, 0 },
                              { Character, 1 }, { Space, .9f }, { Decay, 4.5f }, { Damping, .55f }, { PreDelay, 42 }, { ReverbMix, .3f },
                              { VaultKind, 6 }, { VaultMix, .6f } } },
        },
        {
          { "Altitude", { { HighPass, -.8f, 1.5f }, { Wind, -.2f, .3f }, { Darkness, .1f, 0 } } },
          { "Echo", { { SigAmount, -.6f, 0 }, { TapeMix, -.2f, .2f }, { TapeFeedback, -.2f, .15f } } },
          { "Frost", { { Shimmer, 0, .35f }, { Damping, .2f, -.2f }, { Blur, 0, .3f }, { WraithMix, 0, .15f }, { VaultShimmer, 0, .3f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Necropolis", "Gates",
        // Direction Update 2. The gates of the dead city: wider, darker, deeper. A wide
        // dark vault of a space with weight in the low mids (Black Vault), the rusted
        // iron groaning (its signature), a slow ominous sway, distant dark echoes, a
        // shadow an octave below.
        { { HighPass, 45 }, { Cutoff, 5200 }, { Darkness, .35f }, { FilterMovement, .18f }, { FilterRate, .05f }, { Wind, .12f },
          { WraithMix, .14f }, { Blur, .5f }, { Ghost, .3f },
          { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .12f }, { Formant, .12f }, { ChoirDetune, 10 },
          { EnsMix, .22f }, { EnsDepth, 3.5f }, { EnsRate, .09f },
          { DelayTime, 660 }, { Feedback, .38f }, { DelayDamping, .82f }, { Diffusion, .65f }, { Spread, 1 }, { DelayMix, .24f },
          { Character, 0 }, { Space, .55f }, { Decay, 2.4f }, { Damping, .3f }, { PreDelay, 22 }, { ReverbMix, .16f },
          { VaultKind, 1 }, { VaultMix, .62f }, { VaultTone, 5500 }, { VaultDuck, .15f } },
        {
          // The graveyard: sinister and uneasy. Notes drawn in backwards (a reversed
          // ghost swell and the Reverse Swell signature, with a dissonant tritone
          // shimmer below), a cold, pitch-unstable spectral layer, sparse wavering echoes.
          { "Graveyard", { { HighPass, 210 }, { Cutoff, 7500 }, { Darkness, .2f }, { FilterMovement, 0 }, { Wind, .16f },
                           { WraithMix, .42f }, { Blur, .6f }, { Ghost, .55f }, { WraithDetune, 50 }, { Drift, .9f },
                           { ChoirBlend, 0 }, { EnsMix, 0 },
                           { TapeMix, .3f }, { TapeTime, 610 }, { TapeFeedback, .32f }, { TapeTone, 2600 }, { TapeWow, .9f },
                           { DelayTime, 760 }, { Feedback, .25f }, { DelayDamping, .6f }, { Diffusion, .35f }, { DelayMix, .1f },
                           { Character, 0 }, { Space, .6f }, { Decay, 3.6f }, { Damping, .2f }, { PreDelay, 40 }, { ReverbMix, .22f },
                           { VaultKind, 2 }, { VaultMix, .6f }, { VaultTone, 20000 }, { VaultDuck, 0 } } },
          // The mausoleum (LIKE, kept exactly): a tight, sealed stone tomb, hollow and
          // muffled. Everything the Gates' redesign added is pinned back to before.
          { "Mausoleum", { { HighPass, 50 }, { Cutoff, 4500 }, { Darkness, .35f }, { Wind, 0 }, { Drips, .12f },
                           { WraithMix, .08f }, { Ghost, .4f }, { DelayMix, 0 },
                           { Character, 2 }, { Space, .2f }, { Decay, 1.3f }, { Damping, .6f }, { PreDelay, 3 }, { ReverbMix, .4f },
                           { FilterMovement, 0 }, { FilterRate, .12f }, { ChoirBlend, 0 }, { Voices, 2 }, { Interval, 0 }, { Formant, .4f }, { ChoirDetune, 12 },
                           { EnsMix, 0 }, { EnsDepth, 3 }, { EnsRate, .4f },
                           { DelayTime, 520 }, { Feedback, .22f }, { DelayDamping, .6f }, { Diffusion, .35f }, { Spread, .6f },
                           { VaultKind, 0 }, { VaultMix, 0 }, { VaultTone, 20000 }, { VaultDuck, 0 } } },
        },
        {
          { "Chill", { { HighPass, -.6f, 1 }, { Damping, .15f, -.2f }, { Shimmer, 0, .2f }, { VaultTone, -.8f, .8f } } },
          { "Ghosts", { { WraithMix, -.12f, .3f }, { Ghost, -.25f, .35f }, { Drift, -.2f, .3f }, { TapeWow, -.3f, .1f } } },
          { "Silence", { { ReverbMix, .1f, -.15f }, { DelayMix, .06f, -.08f }, { Space, .15f, -.25f }, { VaultMix, .15f, -.35f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Wizard's Tower", "Stairwell",
        // The spiral stair: narrow, tall, bright stone; echoes circling up and around
        // (its signature), a lamp's flicker.
        { { Cutoff, 9000 }, { HighPass, 140 }, { Flicker, .18f }, { FlickerRate, 3 }, { DelayMix, 0 },
          { Character, 2 }, { Space, .3f }, { Decay, 1.8f }, { Damping, .25f }, { PreDelay, 4 }, { ReverbMix, .3f } },
        {
          // The library: dry, hushed, paper-soft.
          { "Library", { { Cutoff, 7000 }, { Darkness, .2f }, { HighPass, 70 }, { Flicker, .08f },
                         { BloodMix, .12f }, { Age, .15f }, { Dust, .1f },
                         { Character, 0 }, { Space, .3f }, { Decay, .7f }, { Damping, .85f }, { PreDelay, 5 }, { ReverbMix, .16f } } },
          // The alchemy lab: bubbling, strange, resonant glassware and a crooked choir.
          { "Alchemy Lab", { { Cutoff, 6000 }, { Resonance, .3f }, { FilterMovement, .35f }, { FilterRate, 1.2f },
                             { Instability, .3f }, { BloodMix, .2f }, { Drips, .25f },
                             { Voices, 2 }, { Interval, 3 }, { ChoirBlend, .14f }, { ChoirDetune, 35 }, { ChoirMovement, .8f },
                             { DelayTime, 190 }, { Feedback, .35f }, { Descent, 2 }, { DelayMix, .12f },
                             { Character, 0 }, { Space, .35f }, { Decay, 1.4f }, { Damping, .45f }, { ReverbMix, .24f } } },
          // The observatory: open to the night sky; vast, slow-rising, starlight.
          { "Observatory", { { Cutoff, 20000 }, { HighPass, 60 }, { Flicker, 0 }, { CryptMode, 1 }, { RiseTime, 1.5f },
                             { WraithMix, .12f }, { Ghost, .3f }, { Blur, .4f },
                             { DelayTime, 375 }, { Feedback, .3f }, { DelayDamping, .3f }, { DelayMix, .1f },
                             { Character, 3 }, { Space, 1 }, { Decay, 12 }, { Shimmer, .4f }, { Damping, .3f }, { PreDelay, 60 }, { ReverbMix, .46f } } },
        },
        {
          { "Arcana", { { Shimmer, -.1f, .35f }, { ChoirBlend, -.1f, .2f }, { Ghost, 0, .3f } } },
          { "Alchemy", { { FilterMovement, -.2f, .4f }, { Instability, -.2f, .4f }, { SigRate, -1, 1 } } },
          { "Starlight", { { Space, -.3f, .2f }, { Decay, -1, 1 }, { SigAmount, -.5f, .3f } } },
        } });
    return w;
}

const std::vector<LocationDesign> world = buildWorld();

// Milestone 6: every place's signature (Signature.cpp), one kind per place and never
// shared, with its amount. Kinds are numbered in place order (Dungeon scene = 1 ...
// Wizard's Tower, Observatory = 40).
struct SignatureSetting { float amount; };
constexpr std::array<std::array<SignatureSetting, placesPerLocation>, LocationCount> signatureAmounts {{
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 1 } }},   // Dungeon: Corridors, Entrance Hall, Prison Cells, Treasure Room, Final Boss Lair
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 1 } }},   // Castle: Courtyard, Great Hall, Throne Room, Chapel, Tower Top
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Cathedral: Cloister, Nave, Crypt, Bell Tower
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Haunted Forest: Forest Edge, Old Path, Witch's Hut, Ancient Grove
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Caverns: Cave Mouth, Crystal Cavern, Underground Lake, Dragon's Hoard
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Village: Village Green, Tavern, Market Square, Blacksmith
    {{ { 1 }, { 1 }, { 1 }, { 0 }, { 0 } }},   // Swamp: Mire, Bog, Sunken Ruins
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Mountains: Foothills, Mountain Pass, Frozen Peak, Dwarven Hall
    {{ { 1 }, { 1 }, { 1 }, { 0 }, { 0 } }},   // Necropolis: Gates, Graveyard, Mausoleum
    {{ { 1 }, { 1 }, { 1 }, { 1 }, { 0 } }},   // Wizard's Tower: Stairwell, Library, Alchemy Lab, Observatory
}};
int firstSignature(int location) noexcept {
    int kind = 1;
    for (int l = 0; l < location; ++l) kind += 1 + static_cast<int>(world[static_cast<std::size_t>(l)].subs.size());
    return kind;
}

float clampToSpec(P id, float v) noexcept { return std::clamp(v, moduleSpecs[id].min, moduleSpecs[id].max); }
}

int signatureKind(int location, int sub) noexcept {
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    return firstSignature(location) + clampSubLocation(location, sub);
}

const LocationDesign& design(int location) noexcept {
    return world[static_cast<std::size_t>(std::clamp(location, 0, static_cast<int>(LocationCount) - 1))];
}

int subLocationCount(int location) noexcept { return static_cast<int>(design(location).subs.size()); }
int clampSubLocation(int location, int sub) noexcept { return std::clamp(sub, 0, subLocationCount(location)); }

const char* subLocationName(int location, int sub) noexcept {
    const auto& d = design(location);
    sub = clampSubLocation(location, sub);
    return sub == 0 ? d.scene : d.subs[static_cast<std::size_t>(sub - 1)].name;
}

int macroCountFor(int location) noexcept { return static_cast<int>(design(location).macros.size()); }
const char* macroName(int location, int macro) noexcept {
    const auto& d = design(location);
    return macro >= 0 && macro < static_cast<int>(d.macros.size()) ? d.macros[static_cast<std::size_t>(macro)].name : "";
}

Frame placeFrame(int location, int sub) noexcept {
    const auto& d = design(location);
    auto f = neutralFrame();
    for (const auto& c : d.base) f[c.id] = clampToSpec(c.id, c.value);
    sub = clampSubLocation(location, sub);
    if (sub > 0)
        for (const auto& c : d.subs[static_cast<std::size_t>(sub - 1)].changes) f[c.id] = clampToSpec(c.id, c.value);
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    f[SigKind] = static_cast<float>(firstSignature(location) + sub);
    f[SigAmount] = signatureAmounts[static_cast<std::size_t>(location)][static_cast<std::size_t>(sub)].amount;
    return f;
}

void applyMacros(Frame& f, int location, const std::array<float, macroCount>& values) noexcept {
    const auto& d = design(location);
    for (std::size_t m = 0; m < d.macros.size() && m < values.size(); ++m) {
        const float v = std::clamp(values[m], 0.0f, 1.0f);
        if (std::abs(v - 0.5f) < 1.0e-6f) continue;
        const float down = v < 0.5f ? (0.5f - v) * 2.0f : 0.0f, up = v > 0.5f ? (v - 0.5f) * 2.0f : 0.0f;
        for (const auto& t : d.macros[m].targets) {
            const float offset = t.down * down + t.up * up;
            const auto& spec = moduleSpecs[t.id];
            float value = f[t.id];
            if (spec.glide == Glide::Log) value *= std::exp2(offset);
            else if (spec.glide == Glide::Step) value = std::round(value + offset);
            else value += offset;
            f[t.id] = clampToSpec(t.id, value);
        }
    }
}

float placeTrimDb(int location, int sub) noexcept {
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    sub = clampSubLocation(location, sub);
    return levelTrimsDb[static_cast<std::size_t>(location)][static_cast<std::size_t>(sub)];
}
}
