#include "Scenes.h"
#include "Motifs.h"

namespace dc::art {
using namespace motif;

void drawPlace(Plate&, SceneId, bool& drawn) { drawn = false; }

std::vector<Hotspot> sceneHotspots(int location, int) {
    std::vector<Hotspot> spots;
    const int count = subLocationCount(location);
    for (int k = 0; k <= count; ++k)
        spots.push_back({ k, { 200.0f + 560.0f * static_cast<float>(k) / static_cast<float>(std::max(1, count)), 420.0f }, 30.0f });
    return spots;
}

std::vector<LightSource> sceneLights(SceneId) { return {}; }
}
