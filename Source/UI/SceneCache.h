#pragma once
// Renders scenes on a background thread and keeps the most recent ones (Milestone 7;
// painted planes since the visual overhaul). The message thread only asks for and blits
// finished images: it never renders or decodes, so the editor cannot freeze while a
// scene is prepared. Painted scenes (Painted.h) are loaded as planes; places without
// painted art are engraved (Scenes.h). Drop-in art in the override folder wins.
#include "Painted.h"
#include <juce_events/juce_events.h>
#include <deque>
#include <functional>

namespace dc::art {
class SceneCache final : private juce::Thread, private juce::AsyncUpdater {
public:
    // `onReady` runs on the message thread when a requested scene is ready. The cache
    // keeps scenes up to `budgetBytes` (at least the two most recent).
    explicit SceneCache(std::function<void()> onReady, std::size_t budgetBytes = 160u << 20);
    ~SceneCache() override;
    // The scene composited at rest if it is ready at this size, otherwise null (and it
    // is queued). Newer requests are prepared first.
    juce::Image get(SceneId, int width, int height);
    // Its planes (painted scenes; an engraved one has only `back` and `flat`).
    Planes planes(SceneId, int width, int height) const;
    bool ready(SceneId, int width, int height) const;
    void request(SceneId, int width, int height);
    // The scene's light sources (for the animated life), once it is ready.
    std::vector<LightSource> lights(SceneId, int width, int height) const;
    // Prepares synchronously on the calling thread (tests and snapshots only).
    juce::Image renderNow(SceneId, int width, int height);
    std::size_t bytesHeld() const;

private:
    struct Key {
        SceneId id; int width, height;
        bool operator==(const Key& o) const noexcept { return id == o.id && width == o.width && height == o.height; }
    };
    struct Entry { Key key; Planes planes; juce::uint32 used; std::vector<LightSource> lights; };
    void run() override;
    void handleAsyncUpdate() override;
    Planes produce(const Key&, std::vector<LightSource>&) const;
    void store(Entry);
    std::atomic<bool> closing { false };

    std::function<void()> onReady;
    std::size_t budget;
    mutable juce::CriticalSection lock;
    std::deque<Key> queue;
    std::vector<Entry> entries;
    juce::uint32 clock = 0;
    juce::WaitableEvent wake;
};
}
