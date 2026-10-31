#include "SceneCache.h"

namespace dc::art {
SceneCache::SceneCache(std::function<void()> ready, int cap)
    : juce::Thread("DungeonCrawler scenes"), onReady(std::move(ready)), capacity(juce::jmax(2, cap)) {
    startThread(juce::Thread::Priority::low);
}

SceneCache::~SceneCache() {
    closing = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread(4000);
    cancelPendingUpdate();
}

bool SceneCache::ready(SceneId id, int width, int height) const {
    const juce::ScopedLock sl(lock);
    const Key key { id, width, height };
    return std::any_of(entries.begin(), entries.end(), [&](const Entry& e) { return e.key == key; });
}

juce::Image SceneCache::get(SceneId id, int width, int height) {
    {
        const juce::ScopedLock sl(lock);
        const Key key { id, width, height };
        for (auto& e : entries)
            if (e.key == key) { e.used = ++clock; return e.image; }
    }
    request(id, width, height);
    return {};
}

std::vector<LightSource> SceneCache::lights(SceneId id, int width, int height) const {
    const juce::ScopedLock sl(lock);
    const Key key { id, width, height };
    for (const auto& e : entries)
        if (e.key == key) return e.lights;
    return {};
}

void SceneCache::request(SceneId id, int width, int height) {
    if (width <= 0 || height <= 0) return;
    const juce::ScopedLock sl(lock);
    const Key key { id, width, height };
    if (std::any_of(entries.begin(), entries.end(), [&](const Entry& e) { return e.key == key; })) return;
    queue.erase(std::remove(queue.begin(), queue.end(), key), queue.end());
    queue.push_front(key);
    if (queue.size() > 16) queue.pop_back();
    wake.signal();
}

juce::Image SceneCache::produce(const Key& key, std::vector<LightSource>& lights) const {
    const auto file = overrideFile(key.id);
    if (file.existsAsFile()) {
        // Drop-in art: scaled to cover the window (the aspect is the editor's).
        const auto image = juce::ImageFileFormat::loadFrom(file);
        if (image.isValid()) {
            juce::Image out(juce::Image::RGB, key.width, key.height, true, juce::SoftwareImageType());
            juce::Graphics g(out);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(image, out.getBounds().toFloat(), juce::RectanglePlacement::fillDestination);
            return out;   // drop-in art has no animated lights
        }
    }
    return renderScene(key.id, key.width, key.height, &closing, &lights);
}

juce::Image SceneCache::renderNow(SceneId id, int width, int height) {
    std::vector<LightSource> found;
    auto image = produce({ id, width, height }, found);
    const juce::ScopedLock sl(lock);
    entries.push_back({ { id, width, height }, image, ++clock, std::move(found) });
    return image;
}

void SceneCache::run() {
    while (!threadShouldExit()) {
        Key key {};
        bool have = false;
        {
            const juce::ScopedLock sl(lock);
            if (!queue.empty()) { key = queue.front(); queue.pop_front(); have = true; }
        }
        if (!have) { wake.wait(500); continue; }
        std::vector<LightSource> found;
        auto image = produce(key, found);
        if (threadShouldExit() || image.isNull()) break;
        {
            const juce::ScopedLock sl(lock);
            entries.push_back({ key, image, ++clock, std::move(found) });
            while (static_cast<int>(entries.size()) > capacity) {
                auto oldest = std::min_element(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.used < b.used; });
                entries.erase(oldest);
            }
        }
        triggerAsyncUpdate();
    }
}

void SceneCache::handleAsyncUpdate() {
    if (onReady) onReady();
}
}
