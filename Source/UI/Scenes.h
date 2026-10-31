#pragma once
// The engraved art (Milestones 7 and 8): the map and one illustration per place, all
// drawn procedurally into an engraving Plate (Engraving.h), in virtual 960 x 560
// coordinates. Rendering is pure computation: the editor runs it off the message
// thread. Drop-in art: a PNG in the override folder replaces any scene (see
// overrideFile()).
#include "Engraving.h"
#include "World/Locations.h"
#include <array>
#include <vector>

namespace dc::art {
// What can be shown: the map (location -1) or a place (location, sub-location).
struct SceneId {
    int location = -1, sub = 0;
    bool isMap() const noexcept { return location < 0; }
    bool operator==(const SceneId& o) const noexcept { return location == o.location && (location < 0 || sub == o.sub); }
};

// A clickable landmark on the map (virtual coordinates).
struct Landmark { int location; juce::Point<float> position; float radius; };
const std::array<Landmark, LocationCount>& mapLandmarks();
// Points of interest inside a location's scenes: where each sub-location's door, stair
// or feature is (index 0 = the location's own scene, usually a way back out).
struct Hotspot { int sub; juce::Point<float> position; float radius; };
std::vector<Hotspot> sceneHotspots(int location, int sub);
// Light sources in a scene (torches, candles, fires, water glints) for the editor's
// subtle animated life: position, radius, kind (0 torch, 1 candle, 2 water glint, 3 fog band).
struct LightSource { juce::Point<float> position; float radius; int kind; };
std::vector<LightSource> sceneLights(SceneId);

// Draws and renders a scene at a pixel size.
juce::Image renderScene(SceneId, int widthPixels, int heightPixels, const std::atomic<bool>* abort = nullptr);
// The drop-in override: <user application data>/DungeonCrawler/scenes/<location>/<sub>.png
// (and map.png), e.g. scenes/dungeon/treasure-room.png. Returns a non-existent file
// when there is none.
juce::File overrideFile(SceneId);
juce::String sceneSlug(SceneId);
}
