#pragma once
// Renders scenes on a background thread and keeps the most recent ones (Milestone 7).
// The message thread only asks for and blits finished images: it never renders, so the
// editor cannot freeze while an engraving is computed. A drop-in PNG in the override
// folder (art::overrideFile) replaces a procedural scene.
#include "Scenes.h"
#include <juce_events/juce_events.h>
#include <deque>
#include <functional>

namespace dc::art {
class SceneCache final : private juce::Thread, private juce::AsyncUpdater {
public:
    // `onReady` runs on the message thread when a requested scene has been rendered.
    explicit SceneCache(std::function<void()> onReady, int capacity = 8);
    ~SceneCache() override;
    // The image if it is ready at this size, otherwise null (and it is queued). Newer
    // requests are rendered first.
    juce::Image get(SceneId, int width, int height);
    bool ready(SceneId, int width, int height) const;
    void request(SceneId, int width, int height);
    // Renders synchronously on the calling thread (tests and snapshots only).
    juce::Image renderNow(SceneId, int width, int height);

private:
    struct Key {
        SceneId id; int width, height;
        bool operator==(const Key& o) const noexcept { return id == o.id && width == o.width && height == o.height; }
    };
    struct Entry { Key key; juce::Image image; juce::uint32 used; };
    void run() override;
    void handleAsyncUpdate() override;
    static juce::Image produce(const Key&);

    std::function<void()> onReady;
    int capacity;
    mutable juce::CriticalSection lock;
    std::deque<Key> queue;
    std::vector<Entry> entries;
    juce::uint32 clock = 0;
    juce::WaitableEvent wake;
};
}
