#include "SceneCache.h"

namespace dc::art {
SceneCache::SceneCache(std::function<void()> ready, std::size_t budgetBytes)
    : juce::Thread("DungeonCrawler scenes"), onReady(std::move(ready)), budget(budgetBytes) {
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
            if (e.key == key) { e.used = ++clock; return e.planes.flat; }
    }
    request(id, width, height);
    return {};
}

Planes SceneCache::planes(SceneId id, int width, int height) const {
    const juce::ScopedLock sl(lock);
    const Key key { id, width, height };
    for (const auto& e : entries)
        if (e.key == key) return e.planes;
    return {};
}

std::vector<LightSource> SceneCache::lights(SceneId id, int width, int height) const {
    const juce::ScopedLock sl(lock);
    const Key key { id, width, height };
    for (const auto& e : entries)
        if (e.key == key) return e.lights;
    return {};
}

std::size_t SceneCache::bytesHeld() const {
    const juce::ScopedLock sl(lock);
    std::size_t total = 0;
    for (const auto& e : entries) total += e.planes.bytes();
    return total;
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

Planes SceneCache::produce(const Key& key, std::vector<LightSource>& lights) const {
    if (isPainted(key.id)) {
        auto p = loadPainted(key.id, key.width, key.height, &closing);
        if (p.flat.isValid()) {
            lights = paintedLights(key.id);
            return p;
        }
    }
    Planes p;
    const auto file = overrideFile(key.id);
    if (file.existsAsFile()) {
        // Drop-in art for a place without painted art: scaled to cover the window.
        const auto image = juce::ImageFileFormat::loadFrom(file);
        if (image.isValid()) {
            juce::Image out(juce::Image::RGB, key.width, key.height, true, juce::SoftwareImageType());
            juce::Graphics g(out);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(image, out.getBounds().toFloat(), juce::RectanglePlacement::fillDestination);
            p.back = p.flat = out;
            return p;
        }
    }
    p.flat = p.back = renderScene(key.id, key.width, key.height, &closing, &lights);
    return p;
}

void SceneCache::store(Entry entry) {
    const juce::ScopedLock sl(lock);
    entries.push_back(std::move(entry));
    // Keep within the memory budget, but always the two most recent scenes.
    for (;;) {
        std::size_t total = 0;
        for (const auto& e : entries) total += e.planes.bytes();
        if (total <= budget || entries.size() <= 2) break;
        auto oldest = std::min_element(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.used < b.used; });
        entries.erase(oldest);
    }
}

juce::Image SceneCache::renderNow(SceneId id, int width, int height) {
    std::vector<LightSource> found;
    auto planes = produce({ id, width, height }, found);
    auto image = planes.flat;
    juce::uint32 now;
    { const juce::ScopedLock sl(lock); now = ++clock; }
    store({ { id, width, height }, std::move(planes), now, std::move(found) });
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
        auto planes = produce(key, found);
        if (threadShouldExit() || planes.flat.isNull()) break;
        juce::uint32 now;
        { const juce::ScopedLock sl(lock); now = ++clock; }
        store({ key, std::move(planes), now, std::move(found) });
        triggerAsyncUpdate();
    }
}

void SceneCache::handleAsyncUpdate() {
    if (onReady) onReady();
}
}
