#pragma once
// Hover that never sticks (Direction Update 2). In GarageBand a control could stay
// highlighted after the mouse had left it (a missed exit, or the control hidden and shown
// again under the mouse during a transition). Two guards:
//  - HoverButton keeps its own hover flag, set on enter, cleared on exit and whenever it is
//    hidden, and paints from it;
//  - sweepHover() runs on every mouse move, enter and exit anywhere in a window: any button
//    that still shows a hover the mouse is no longer over is cleared.
#include <juce_gui_basics/juce_gui_basics.h>

namespace dc::ui {

class HoverButton : public juce::Button {
public:
    explicit HoverButton(const juce::String& name) : juce::Button(name) {}
    bool hot() const noexcept { return hovered; }
    void setHot(bool on) {
        if (on == hovered) return;
        hovered = on;
        repaint();
        if (onHover) onHover();
    }
    std::function<void()> onHover;   // the hover flag changed
    void mouseEnter(const juce::MouseEvent& e) override { juce::Button::mouseEnter(e); setHot(true); }
    void mouseExit(const juce::MouseEvent& e) override { juce::Button::mouseExit(e); setHot(false); }
    void visibilityChanged() override {
        juce::Button::visibilityChanged();
        if (!isVisible()) setHot(false);
    }
    void paintButton(juce::Graphics& g, bool, bool down) override { paintHover(g, hovered && isEnabled(), down); }
    virtual void paintHover(juce::Graphics&, bool hover, bool down) = 0;

private:
    bool hovered = false;
};

// Clears every stale hover under `root` for a mouse at `screen` (screen coordinates).
// Cheap: a walk over the window's few dozen buttons, run only on mouse events.
inline int sweepHover(juce::Component& root, juce::Point<float> screen) {
    int cleared = 0;
    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
        for (auto* child : c.getChildren()) {
            if (auto* b = dynamic_cast<juce::Button*>(child)) {
                const bool inside = b->isShowing() && b->reallyContains(b->getLocalPoint(nullptr, screen).roundToInt(), true);
                if (!inside) {
                    if (auto* h = dynamic_cast<HoverButton*>(b); h != nullptr && h->hot()) { h->setHot(false); ++cleared; }
                    if (b->getState() == juce::Button::buttonOver) { b->setState(juce::Button::buttonNormal); ++cleared; }
                }
            }
            walk(*child);
        }
    };
    walk(root);
    return cleared;
}

} // namespace dc::ui
