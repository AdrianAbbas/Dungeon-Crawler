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
          // A vaulted entrance: bigger and lighter, the torches brighter, no slap.
          { "Entrance Hall", { { Cutoff, 6000 }, { Darkness, .3f }, { HighPass, 45 }, { Flicker, .45f }, { FlickerRate, 2.6f }, { Drips, .06f },
                               { DelayTime, 310 }, { Feedback, .1f }, { Diffusion, .2f }, { DelayDamping, .6f }, { Spread, .9f }, { DelayMix, .1f },
                               { Character, 2 }, { Space, .9f }, { Decay, 3.5f }, { Damping, .55f }, { PreDelay, 38 }, { ReverbMix, .45f } } },
          // A cramped cell: boxy and muffled, iron bars ringing, a flutter between the
          // walls, grime on everything.
          { "Prison Cells", { { Cutoff, 1900 }, { Resonance, .25f }, { Darkness, .45f }, { HighPass, 130 },
                              { Drive, 5 }, { Age, .35f }, { Dust, .12f }, { BloodMix, .35f },
                              { Metal, .42f }, { MetalKind, 0 }, { MetalNote, 2 }, { MetalOctave, 2 }, { MetalDecay, .7f }, { Drips, .3f }, { Flicker, .12f },
                              { DelayTime, 55 }, { Feedback, .42f }, { Diffusion, .15f }, { DelayDamping, .5f }, { Spread, .3f }, { DelayMix, .16f },
                              { Character, 0 }, { Space, .08f }, { Decay, .7f }, { Damping, .7f }, { PreDelay, 2 }, { ReverbMix, .38f } } },
          // Gold everywhere: bright glittering reflections, crystal glints, an octave of
          // sparkle above, shimmer, ping-pong echoes on the eighth.
          { "Treasure Room", { { Cutoff, 20000 }, { Darkness, 0 }, { HighPass, 90 }, { Resonance, 0 }, { BloodMix, 0 }, { Drive, 0 }, { Age, 0 },
                               { Metal, .22f }, { MetalKind, 2 }, { MetalNote, 9 }, { MetalOctave, 4 }, { MetalDecay, 1.4f }, { Flicker, .1f }, { Drips, 0 },
                               { Voices, 1 }, { Interval, 4 }, { ChoirBlend, .12f }, { Formant, .85f }, { ChoirDetune, 6 },
                               { DelayTime, 250 }, { DelaySync, 2 }, { Feedback, .38f }, { Spread, 1 }, { DelayDamping, .12f }, { Diffusion, .2f }, { DelayMix, .16f },
                               { Character, 0 }, { Space, .55f }, { Decay, 3 }, { Damping, .12f }, { PreDelay, 18 }, { Shimmer, .4f }, { ReverbMix, .38f } } },
          // The lair: vast and menacing. A distorted, darkened instrument, a choir a
          // fifth below with a held drone, a low resonance, slow echoes that sink a
          // semitone each time, an endless void of a room.
          { "Final Boss Lair", { { Cutoff, 3000 }, { Darkness, .55f }, { HighPass, 30 }, { Drive, 18 }, { BloodMix, .55f }, { Texture, .6f },
                                 { Voices, 3 }, { Interval, 1 }, { ChoirBlend, .3f }, { Formant, .2f }, { ChoirDetune, 22 }, { Drone, .22f },
                                 { Tomb, .25f }, { TombNote, 2 }, { TombOctave, 1 }, { Flicker, .3f }, { FlickerRate, 1.6f }, { Drips, 0 },
                                 { WraithMix, .1f }, { Blur, .5f },
                                 { DelayTime, 760 }, { Feedback, .45f }, { DelayDamping, .65f }, { Descent, 1 }, { Diffusion, .5f }, { DelayMix, .2f },
                                 { Character, 3 }, { Space, .95f }, { Decay, 8 }, { Damping, .55f }, { PreDelay, 60 }, { ReverbMix, .45f } } },
        },
        {
          { "Depth", { { Space, -.35f, .35f }, { Decay, -1, 1 }, { PreDelay, -12, 30 }, { ReverbMix, -.15f, .15f } } },
          { "Damp", { { Damping, -.3f, .3f }, { Cutoff, 1.5f, -1.5f }, { Darkness, -.2f, .3f }, { Drips, -.25f, .4f } } },
          { "Echo", { { DelayMix, -.12f, .2f }, { Feedback, -.2f, .25f }, { DelayTime, -.5f, .6f } } },
          { "Dread", { { Drive, -4, 12 }, { BloodMix, -.2f, .3f }, { Drone, 0, .35f }, { Tomb, 0, .3f }, { Cutoff, 1, -.7f }, { Darkness, -.3f, .2f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Castle", "Courtyard",
        // An open courtyard between high walls: a crisp slap echo off the far wall,
        // a short open reverb, a breeze.
        { { HighPass, 70 }, { Darkness, .05f }, { Wind, .22f },
          { DelayTime, 210 }, { Feedback, .1f }, { Diffusion, .1f }, { DelayDamping, .3f }, { Spread, 1 }, { DelayMix, .22f },
          { Character, 0 }, { Space, .6f }, { Decay, 1.5f }, { Damping, .35f }, { PreDelay, 22 }, { ReverbMix, .24f } },
        {
          // Timber roof over stone: warm, wide, medium-large, a fire in the hearth.
          { "Great Hall", { { HighPass, 35 }, { Cutoff, 10000 }, { Darkness, .12f }, { Drive, 3 }, { Texture, .2f }, { BloodMix, .15f },
                            { Flicker, .1f }, { FlickerRate, 2.2f }, { Wind, 0 },
                            { DelayMix, 0 },
                            { Character, 1 }, { Space, .7f }, { Decay, 2.8f }, { Damping, .62f }, { PreDelay, 20 }, { ReverbMix, .4f } } },
          // Regal: a grand, bright, very wide hall, an organ-like fifth above, a halo.
          { "Throne Room", { { HighPass, 40 }, { Wind, 0 },
                             { Voices, 2 }, { Interval, 3 }, { ChoirBlend, .11f }, { Formant, .5f }, { ChoirDetune, 8 }, { ChoirMovement, .2f },
                             { DelayTime, 260 }, { Feedback, .18f }, { Diffusion, .6f }, { DelayMix, .07f },
                             { Character, 1 }, { Space, .95f }, { Decay, 4.2f }, { Damping, .3f }, { PreDelay, 50 }, { Shimmer, .14f }, { ReverbWidth, 1 }, { ReverbMix, .45f } } },
          // Sacred: a long reverb and a soft choir an octave below with a drone.
          { "Chapel", { { Cutoff, 8500 }, { Darkness, .1f }, { Wind, 0 },
                        { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .22f }, { Formant, .55f }, { ChoirMovement, .4f }, { Drone, .12f }, { DroneHold, 6 },
                        { DelayMix, 0 },
                        { Character, 1 }, { Space, .85f }, { Decay, 6.5f }, { Damping, .5f }, { PreDelay, 45 }, { Shimmer, .08f }, { ReverbMix, .5f } } },
          // On the battlements: thin, windy, open, distant echoes across the valley.
          { "Tower Top", { { HighPass, 230 }, { Cutoff, 6000 }, { Darkness, .2f }, { Wind, .42f },
                           { WraithMix, .12f }, { Blur, .6f },
                           { DelayTime, 520 }, { Feedback, .35f }, { DelayDamping, .6f }, { Diffusion, .5f }, { Spread, 1 }, { DelayMix, .26f },
                           { Character, 3 }, { Space, .35f }, { Decay, 2 }, { Damping, .5f }, { PreDelay, 100 }, { ReverbLowCut, 320 }, { ReverbMix, .2f } } },
        },
        {
          { "Hall Size", { { Space, -.35f, .3f }, { Decay, -1, 1 }, { PreDelay, -15, 30 } } },
          { "Grandeur", { { Shimmer, -.1f, .3f }, { ChoirBlend, -.1f, .2f }, { ReverbWidth, -.4f, 0 }, { Drone, 0, .15f } } },
          { "Distance", { { ReverbMix, -.2f, .25f }, { Cutoff, 1, -1.5f }, { DelayMix, -.08f, .12f }, { HighPass, -.5f, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Cathedral", "Cloister",
        // Arcades around a garden: a flutter of reflections between the arches,
        // feeding a long stone reverb.
        { { Cutoff, 12000 }, { Darkness, .05f }, { Wind, .1f },
          { DelayTime, 92 }, { Feedback, .45f }, { Diffusion, .7f }, { DelayDamping, .4f }, { Spread, .6f }, { DelayMix, .16f },
          { Character, 1 }, { Space, .6f }, { Decay, 4.5f }, { Damping, .5f }, { PreDelay, 28 }, { ReverbMix, .38f } },
        {
          // The nave: the longest, wettest place in the world; a breath of choir.
          { "Nave", { { Cutoff, 10000 }, { Wind, 0 }, { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .06f }, { Formant, .55f }, { DelayMix, 0 },
                      { Character, 1 }, { Space, 1 }, { Decay, 12 }, { Damping, .45f }, { PreDelay, 80 }, { Shimmer, .05f }, { ReverbMix, .6f } } },
          // Below the floor: very dark and close, a low resonance, a murmur an octave down.
          { "Crypt", { { Cutoff, 1500 }, { Darkness, .62f }, { HighPass, 28 }, { Wind, 0 }, { Drips, .14f },
                       { Tomb, .32f }, { TombNote, 2 }, { TombOctave, 1 }, { Drive, 3 }, { Age, .15f }, { BloodMix, .2f },
                       { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .15f }, { Formant, .1f }, { DelayMix, 0 },
                       { Character, 2 }, { Space, .3f }, { Decay, 2 }, { Damping, .85f }, { PreDelay, 6 }, { ReverbMix, .45f } } },
          // In the belfry: the instrument strikes bells, wind through the louvres.
          { "Bell Tower", { { HighPass, 80 }, { Cutoff, 8000 }, { Darkness, .12f }, { Wind, .25f },
                            { Metal, .6f }, { MetalKind, 1 }, { MetalNote, 2 }, { MetalOctave, 1 }, { MetalDecay, 5 },
                            { DelayTime, 420 }, { Feedback, .25f }, { Diffusion, .4f }, { DelayMix, .12f },
                            { Character, 0 }, { Space, .45f }, { Decay, 2.1f }, { Damping, .35f }, { PreDelay, 40 }, { Shimmer, .08f }, { ReverbMix, .3f } } },
        },
        {
          { "Vastness", { { Space, -.4f, 0 }, { Decay, -1.3f, .8f }, { PreDelay, -20, 40 }, { ReverbMix, -.15f, .12f } } },
          { "Choir", { { ChoirBlend, -.15f, .25f }, { Drone, 0, .2f }, { Voices, 0, 1 } } },
          { "Bells", { { Metal, -.3f, .4f }, { MetalDecay, -1, 1 }, { Shimmer, -.08f, .2f } } },
          { "Gloom", { { Cutoff, 1, -2 }, { Darkness, -.15f, .4f }, { Damping, -.2f, .3f } } },
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
        // The mouth of the cave: big rock, discrete echoes from deep inside, drips.
        { { HighPass, 60 }, { Cutoff, 9000 }, { Darkness, .1f }, { Drips, .35f }, { Wind, .12f },
          { DelayTime, 290 }, { Feedback, .42f }, { Diffusion, .25f }, { DelayDamping, .35f }, { Spread, .85f }, { DelayMix, .22f },
          { Character, 1 }, { Space, .8f }, { Decay, 4.5f }, { Damping, .3f }, { PreDelay, 55 }, { ReverbMix, .38f } },
        {
          // Crystal: everything rings, cold and bright; long glassy echoes.
          { "Crystal Cavern", { { Cutoff, 20000 }, { HighPass, 180 }, { Darkness, 0 }, { Wind, 0 },
                                { Metal, .7f }, { MetalKind, 2 }, { MetalNote, 4 }, { MetalOctave, 3 }, { MetalDecay, 4.5f }, { Drips, .5f },
                                { DelayTime, 230 }, { Feedback, .45f }, { DelayDamping, .08f }, { Diffusion, .1f }, { Spread, 1 }, { DelayMix, .16f },
                                { Character, 1 }, { Space, .6f }, { Decay, 5 }, { Damping, .08f }, { PreDelay, 30 }, { Shimmer, .12f }, { ReverbMix, .36f } } },
          // A still lake under the rock: watery, filtered, wavering, very long.
          { "Underground Lake", { { Cutoff, 2200 }, { Resonance, .35f }, { FilterMovement, .45f }, { FilterRate, .12f }, { Darkness, .3f }, { Wind, 0 },
                                  { Instability, .45f }, { BloodMix, .25f }, { Drips, .6f },
                                  { WraithMix, .2f }, { Blur, .6f }, { Drift, .6f },
                                  { DelayTime, 650 }, { Feedback, .55f }, { DelayDamping, .8f }, { Diffusion, .85f }, { DelayMix, .25f },
                                  { Character, 3 }, { Space, .8f }, { Decay, 8 }, { Damping, .75f }, { PreDelay, 80 }, { ReverbMix, .48f } } },
          // The hoard: massive and dark, a rumbling sub-octave drone and resonance,
          // the glint of gold above it.
          { "Dragon's Hoard", { { Cutoff, 2400 }, { Darkness, .6f }, { HighPass, 25 }, { Wind, 0 }, { Drips, 0 },
                                { Drive, 8 }, { BloodMix, .35f }, { Texture, .5f },
                                { Voices, 2 }, { Interval, 0 }, { ChoirBlend, .2f }, { Formant, .12f }, { Drone, .26f }, { DroneHold, 10 },
                                { Tomb, .3f }, { TombNote, 2 }, { TombOctave, 0 },
                                { Metal, .12f }, { MetalKind, 2 }, { MetalNote, 2 }, { MetalOctave, 4 }, { MetalDecay, 1.5f },
                                { DelayMix, 0 },
                                { Character, 1 }, { Space, 1 }, { Decay, 7 }, { Damping, .7f }, { PreDelay, 70 }, { ReverbMix, .46f } } },
        },
        {
          { "Depth", { { Space, -.35f, .15f }, { Decay, -1, 1 }, { PreDelay, -25, 40 } } },
          { "Drips", { { Drips, -.35f, .45f } } },
          { "Echo", { { DelayMix, -.15f, .2f }, { Feedback, -.25f, .25f }, { DelayTime, -.6f, .6f } } },
          { "Rumble", { { Drone, 0, .35f }, { Drive, -4, 10 }, { BloodMix, -.15f, .25f }, { Tomb, 0, .25f } } },
        } });

    // Milestone 6 sound design (Village, Swamp, Mountains, Necropolis, Wizard's Tower).
    // Each place's signature (Signature.cpp) sits on top of the room designed here.
    //--------------------------------------------------------------------------
    w.push_back({ "Village", "Village Green",
        // The green: open air among cottages, light and lively; a second player's
        // doubling (Folk Chorus), a little breeze, a small open reverb.
        { { HighPass, 45 }, { Wind, .1f },
          { DelayTime, 160 }, { Feedback, .08f }, { Diffusion, .4f }, { DelayDamping, .45f }, { Spread, .9f }, { DelayMix, .08f },
          { Character, 0 }, { Space, .4f }, { Decay, 1.1f }, { Damping, .45f }, { PreDelay, 12 }, { ReverbMix, .16f } },
        {
          // A low-beamed tavern: a small, warm, wooden room by the fire.
          { "Tavern", { { Cutoff, 9000 }, { Darkness, .15f }, { HighPass, 60 }, { Wind, 0 }, { Flicker, .12f }, { FlickerRate, 2.4f },
                        { Tomb, .1f }, { TombNote, 7 }, { TombOctave, 1 }, { DelayMix, 0 },
                        { Character, 0 }, { Space, .18f }, { Decay, .75f }, { Damping, .72f }, { PreDelay, 4 }, { ReverbMix, .26f } } },
          // The market square: open and bright among the buildings (the slap-back is
          // its signature), a busier breeze.
          { "Market Square", { { HighPass, 80 }, { Wind, .14f }, { DelayMix, 0 },
                               { Character, 1 }, { Space, .55f }, { Decay, 1.5f }, { Damping, .3f }, { PreDelay, 24 }, { ReverbWidth, 1 }, { ReverbMix, .17f } } },
          // The smithy: hot, gritty, a hard little room with the forge flickering.
          { "Blacksmith", { { Cutoff, 12000 }, { HighPass, 70 }, { Wind, 0 }, { Drive, 9 }, { BloodMix, .4f }, { Texture, .75f }, { Dust, .18f },
                            { Flicker, .4f }, { FlickerRate, 6.5f }, { DelayMix, 0 },
                            { Character, 2 }, { Space, .28f }, { Decay, 1.1f }, { Damping, .3f }, { PreDelay, 6 }, { ReverbMix, .24f } } },
        },
        {
          { "Warmth", { { Drive, -3, 6 }, { Texture, -.15f, .3f }, { Cutoff, 1, -1 }, { Damping, -.2f, .25f } } },
          { "Bustle", { { DelayMix, -.08f, .15f }, { Space, -.15f, .25f }, { ReverbMix, -.1f, .15f }, { SigAmount, -.4f, .3f } } },
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
        // Open hills: almost dry, the wind, and one long answer from across the valley
        // (its signature).
        { { HighPass, 65 }, { Wind, .28f }, { DelayMix, 0 },
          { Character, 0 }, { Space, .3f }, { Decay, .9f }, { Damping, .5f }, { PreDelay, 10 }, { ReverbMix, .1f } },
        {
          // The pass: rock walls on both sides, discrete echoes receding down the canyon.
          { "Mountain Pass", { { HighPass, 100 }, { Wind, .18f },
                               { Character, 3 }, { Space, .6f }, { Decay, 2.6f }, { Damping, .25f }, { PreDelay, 70 }, { ReverbLowCut, 220 }, { ReverbMix, .18f } } },
          // The peak: thin, cold, airy; ice above everything, sparse echoes, wind.
          { "Frozen Peak", { { HighPass, 260 }, { Cutoff, 16000 }, { Wind, .38f },
                             { WraithMix, .14f }, { Blur, .4f }, { Ghost, .2f },
                             { Character, 3 }, { Space, .45f }, { Decay, 2.6f }, { Damping, .08f }, { PreDelay, 90 }, { Shimmer, .22f }, { ReverbLowCut, 400 }, { ReverbMix, .26f } } },
          // The dwarven hall: deep stone carved into the mountain, warm with the forge,
          // a low hum of voices.
          { "Dwarven Hall", { { HighPass, 28 }, { Cutoff, 6500 }, { Darkness, .28f }, { Wind, 0 }, { Drive, 4 }, { BloodMix, .18f }, { Flicker, .22f }, { FlickerRate, 2 },
                              { Voices, 1 }, { Interval, 0 }, { ChoirBlend, .1f }, { Formant, .2f },
                              { Character, 1 }, { Space, .9f }, { Decay, 4.5f }, { Damping, .55f }, { PreDelay, 42 }, { ReverbMix, .42f } } },
        },
        {
          { "Altitude", { { HighPass, -.8f, 1.5f }, { Wind, -.2f, .3f }, { Darkness, .1f, 0 } } },
          { "Echo", { { SigAmount, -.6f, 0 }, { DelayMix, 0, .15f }, { Feedback, 0, .3f } } },
          { "Frost", { { Shimmer, 0, .35f }, { Damping, .2f, -.2f }, { Blur, 0, .3f }, { WraithMix, 0, .15f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Necropolis", "Gates",
        // The gates of the dead city: cold air, rusted iron groaning (its signature), a
        // faint ghostly haze, a hard stone reverb.
        { { HighPass, 95 }, { Darkness, .12f }, { Wind, .16f },
          { WraithMix, .14f }, { Blur, .5f }, { Ghost, .3f },
          { DelayTime, 520 }, { Feedback, .22f }, { DelayDamping, .6f }, { DelayMix, .08f },
          { Character, 0 }, { Space, .55f }, { Decay, 2.4f }, { Damping, .3f }, { PreDelay, 22 }, { ReverbMix, .24f } },
        {
          // The graveyard: open, cold and sparse; ghosts of every note, swells drawn in
          // backwards.
          { "Graveyard", { { HighPass, 140 }, { Cutoff, 11000 }, { WraithMix, .32f }, { Blur, .55f }, { Ghost, .5f }, { WraithDetune, 30 }, { Drift, .6f },
                           { DelayTime, 760 }, { Feedback, .25f }, { DelayMix, .1f },
                           { Space, .6f }, { Decay, 3.6f }, { Damping, .2f }, { PreDelay, 40 }, { ReverbMix, .28f } } },
          // The mausoleum: a tight, sealed stone tomb, hollow and muffled.
          { "Mausoleum", { { HighPass, 50 }, { Cutoff, 4500 }, { Darkness, .35f }, { Wind, 0 }, { Drips, .12f },
                           { WraithMix, .08f }, { Ghost, .4f }, { DelayMix, 0 },
                           { Character, 2 }, { Space, .2f }, { Decay, 1.3f }, { Damping, .6f }, { PreDelay, 3 }, { ReverbMix, .4f } } },
        },
        {
          { "Chill", { { HighPass, -.6f, 1 }, { Damping, .15f, -.2f }, { Shimmer, 0, .2f } } },
          { "Ghosts", { { WraithMix, -.12f, .3f }, { Ghost, -.25f, .35f }, { Drift, -.2f, .3f } } },
          { "Silence", { { ReverbMix, .1f, -.15f }, { DelayMix, .06f, -.08f }, { Space, .15f, -.25f } } },
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
