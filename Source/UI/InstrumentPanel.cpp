#include "InstrumentPanel.h"

namespace dc::ui {
namespace {
bool isBlack(int key) {
    const int pc = key % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}
int whiteIndex(int key) {
    static constexpr int inOctave[12] { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    return (key / 12) * 7 + inOctave[key % 12];
}
}

InstrumentPanel::InstrumentPanel(DungeonCrawlerProcessor& p) : processor(p), keyboard(p) {
    setOpaque(false);
    close.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    close.setTooltip("Close (Esc)");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(close);

    lock.setTooltip("Keep this instrument while you travel to other places");
    lock.setClickingTogglesState(true);
    lock.onClick = [this] {
        // Locking keeps what plays now.
        processor.setInstrumentLock(lock.getToggleState());
        refresh();
    };
    addAndMakeVisible(lock);

    // The chooser: every instrument, under its location. Choosing one locks it.
    for (int l = 0; l < dc::LocationCount; ++l) {
        choose.addSectionHeading(dc::locationNames[std::size_t(l)]);
        for (int k = 0; k <= dc::subLocationCount(l); ++k) {
            const int index = dc::inst::designFor(l, k);
            choose.addItem(juce::String(dc::inst::design(index).name) + "  (" + dc::subLocationName(l, k) + ")", index + 1);
        }
    }
    choose.setTextWhenNothingSelected("Choose an instrument");
    choose.setTooltip("Any instrument, in any place (locks it)");
    choose.onChange = [this] {
        const int index = choose.getSelectedId() - 1;
        if (index >= 0 && index != processor.playingInstrument.load()) processor.chooseInstrument(index);
        refresh();
    };
    addAndMakeVisible(choose);

    for (std::size_t m = 0; m < macros.size(); ++m) setUpFader(macros[m], dc::ids::instMacros[m], "Macro", "");
    setUpFader(intensity, dc::ids::intensity, "Intensity", "How strongly the place's effects apply");
    setUpFader(space, dc::ids::mix, "Space", "The place's space and effects against the dry instrument");

    bendLabel.setText("Bend", juce::dontSendNotification);
    bendLabel.setFont(labelFont(14.0f));
    bendLabel.setColour(juce::Label::textColourId, colours::ash);
    addAndMakeVisible(bendLabel);
    for (int st = 1; st <= 24; ++st) bendRange.addItem(juce::String::fromUTF8("\xc2\xb1") + juce::String(st) + " st", st);
    bendRange.setTooltip("Pitch bend range");
    bendAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*processor.parameters.getParameter(dc::ids::bendRange), bendRange);
    addAndMakeVisible(bendRange);

    wheels.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(wheels);
    addAndMakeVisible(keyboard);
    refresh();
}

InstrumentPanel::~InstrumentPanel() { stopTimer(); }

void InstrumentPanel::setUpFader(Fader& fader, const juce::String& id, const juce::String& name, const juce::String& tip) {
    auto* parameter = processor.parameters.getParameter(id);
    fader.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 15);
    fader.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    fader.slider.setDoubleClickReturnValue(true, double(parameter->convertFrom0to1(parameter->getDefaultValue())));
    fader.attachment = std::make_unique<juce::SliderParameterAttachment>(*parameter, fader.slider);
    fader.label.setText(name, juce::dontSendNotification);
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setFont(labelFont(14.5f));
    fader.label.setColour(juce::Label::textColourId, colours::ivory.withAlpha(0.92f));
    if (tip.isNotEmpty()) fader.slider.setTooltip(tip);
    addAndMakeVisible(fader.slider);
    addAndMakeVisible(fader.label);
}

void InstrumentPanel::refresh() {
    const int index = processor.isInstrument() ? processor.playingInstrument.load() : 0;
    const auto& d = dc::inst::design(index);
    const int l = dc::inst::locationOf(index), k = dc::inst::subOf(index);
    shownInstrument = index;
    shownPlaceCode = processor.currentDestination.load() * 8 + processor.currentSubLocation.load();
    shownLock = processor.instrumentLocked();
    shownTitle = d.name;
    shownPlace = juce::String(dc::locationNames[std::size_t(l)]) + juce::String::fromUTF8("  \xc2\xb7  ") + dc::subLocationName(l, k);
    const int here = processor.currentDestination.load(), hereSub = processor.currentSubLocation.load();
    if (shownLock && dc::inst::designFor(here, hereSub) != index)
        shownPlace << juce::String::fromUTF8("   \xe2\x80\x94   played in ") << dc::subLocationName(here, hereSub);
    shownWheel = d.modLabel;
    static constexpr const char* tips[dc::inst::macroCount] { "Brighter or darker", "How it starts: softer or harder", "How long it rings after the key",
                                                              "The instrument's body", "The instrument's own character" };
    for (std::size_t m = 0; m < macros.size(); ++m) {
        macros[m].label.setText(d.macros[m], juce::dontSendNotification);
        macros[m].slider.setTooltip(juce::String(d.macros[m]) + ": " + tips[m]);
    }
    choose.setSelectedId(index + 1, juce::dontSendNotification);
    lock.setToggleState(shownLock, juce::dontSendNotification);
    repaint();
}

void InstrumentPanel::visibilityChanged() {
    if (isVisible()) {
        refresh();
        startTimerHz(15);
    } else {
        stopTimer();
    }
}

void InstrumentPanel::timerCallback() {
    // The instrument (a new place, a preset, automation).
    const int place = processor.currentDestination.load() * 8 + processor.currentSubLocation.load();
    if (processor.playingInstrument.load() != shownInstrument || processor.instrumentLocked() != shownLock || place != shownPlaceCode) refresh();
    // Keys: repaint the keyboard only when what is lit changed.
    std::array<uint64_t, 2> now {};
    const auto& engine = processor.instrumentEngine();
    for (int key = 0; key < 128; ++key)
        if (engine.keyLit(key)) now[std::size_t(key >> 6)] |= uint64_t(1) << (key & 63);
    if (now != keyboard.lit) {
        keyboard.lit = now;
        ++repaintRequests;
        keyboard.repaint();
    }
    const float bend = processor.shownBend.load(), mod = processor.shownMod.load();
    const bool pedal = processor.shownSustain.load();
    if (std::abs(bend - wheels.bend) > 0.01f || std::abs(mod - wheels.mod) > 0.01f || pedal != wheels.pedal) {
        wheels.bend = bend;
        wheels.mod = mod;
        wheels.pedal = pedal;
        ++repaintRequests;
        wheels.repaint();
    }
}

void InstrumentPanel::paint(juce::Graphics& g) {
    g.setColour(colours::ink.withAlpha(0.95f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
    // A crimson rule under the title, as the place names have.
    g.setColour(colours::blood.withAlpha(0.8f));
    g.fillRect(14.0f, 40.0f, 180.0f, 1.0f);
    g.setColour(colours::ivory);
    g.setFont(titleFont(20.0f));
    g.drawText(shownTitle, 14, 8, 380, 28, juce::Justification::centredLeft);
    g.setColour(colours::ash);
    g.setFont(labelFont(15.0f));
    g.drawText(shownPlace, 14, 44, 520, 18, juce::Justification::centredLeft);
    g.setColour(colours::dust);
    g.setFont(labelFont(13.0f));
    g.drawText(juce::String("Mod wheel: ") + shownWheel, wheels.getX() - 4, wheels.getBottom() + 2, wheels.getWidth() + 60, 16, juce::Justification::centredLeft);
}

void InstrumentPanel::resized() {
    const int w = getWidth();
    close.setBounds(w - 34, 6, 28, 24);
    choose.setBounds(w - 330, 8, 286, 24);
    lock.setBounds(w - 480, 8, 140, 24);
    const int faderTop = 66, faderHeight = 112;
    const auto place = [](Fader& f, juce::Rectangle<int> r) {
        f.label.setBounds(r.removeFromBottom(17));
        f.slider.setBounds(r.reduced(2, 0));
    };
    for (std::size_t m = 0; m < macros.size(); ++m) place(macros[m], { 14 + int(m) * 66, faderTop, 66, faderHeight });
    place(intensity, { 14 + 5 * 66 + 26, faderTop, 70, faderHeight });
    place(space, { 14 + 5 * 66 + 96, faderTop, 70, faderHeight });
    bendLabel.setBounds(w - 300, faderTop + 2, 50, 22);
    bendRange.setBounds(w - 250, faderTop + 2, 110, 22);
    wheels.setBounds(w - 300, faderTop + 30, 180, 62);
    keyboard.setBounds(14, faderTop + faderHeight + 10, w - 28, getHeight() - (faderTop + faderHeight + 10) - 12);
}

//==============================================================================
void InstrumentPanel::Wheels::paint(juce::Graphics& g) {
    // Two slim vertical tracks (bend centred, mod from the bottom) and the pedal lamp.
    const auto track = [&](float x, const char* name) {
        g.setColour(colours::charcoal);
        g.fillRoundedRectangle(x, 0.0f, 12.0f, float(getHeight()) - 16.0f, 3.0f);
        g.setColour(colours::ash);
        g.setFont(labelFont(12.0f));
        g.drawText(name, int(x) - 14, getHeight() - 15, 40, 14, juce::Justification::centred);
    };
    const float h = float(getHeight()) - 16.0f;
    track(6.0f, "Bend");
    track(52.0f, "Mod");
    g.setColour(colours::blood);
    const float mid = h * 0.5f, y = mid - bend * mid;
    g.fillRect(7.0f, std::min(mid, y), 10.0f, std::max(2.0f, std::abs(y - mid)));
    g.setColour(colours::ivory.withAlpha(0.6f));
    g.fillRect(4.0f, mid - 0.5f, 16.0f, 1.0f);
    g.setColour(colours::blood);
    g.fillRect(53.0f, h - mod * h, 10.0f, mod * h);
    g.setColour(pedal ? colours::ember : colours::charcoal.brighter(0.3f));
    g.fillEllipse(110.0f, h * 0.5f - 7.0f, 14.0f, 14.0f);
    g.setColour(colours::ash);
    g.setFont(labelFont(12.0f));
    g.drawText("Pedal", 96, getHeight() - 15, 44, 14, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<float> InstrumentPanel::Keyboard::keyBounds(int key) const {
    const int whites = whiteIndex(highest) - whiteIndex(lowest) + 1;
    const float ww = float(getWidth()) / float(whites), h = float(getHeight());
    const float x = float(whiteIndex(key) - whiteIndex(lowest)) * ww;
    if (!isBlack(key)) return { x, 0.0f, ww, h };
    return { x + ww * 0.68f, 0.0f, ww * 0.64f, h * 0.62f };
}

int InstrumentPanel::Keyboard::keyAt(juce::Point<float> p) const {
    for (int key = lowest; key <= highest; ++key)
        if (isBlack(key) && keyBounds(key).contains(p)) return key;
    for (int key = lowest; key <= highest; ++key)
        if (!isBlack(key) && keyBounds(key).contains(p)) return key;
    return -1;
}

void InstrumentPanel::Keyboard::paint(juce::Graphics& g) {
    ++repaints;
    const auto on = [this](int key) { return ((lit[std::size_t(key >> 6)] >> (key & 63)) & 1u) != 0; };
    for (int key = lowest; key <= highest; ++key) {
        if (isBlack(key)) continue;
        const auto r = keyBounds(key).reduced(0.5f, 0.0f);
        g.setColour(on(key) ? colours::blood.brighter(0.15f) : colours::ivory.withAlpha(0.88f));
        g.fillRect(r);
        g.setColour(colours::ink);
        g.drawRect(r, 1.0f);
        if (key % 12 == 0) {
            g.setColour(colours::dust);
            g.setFont(labelFont(11.0f));
            g.drawText("C" + juce::String(key / 12 - 2), r.withTrimmedTop(r.getHeight() - 16.0f).toNearestInt(), juce::Justification::centred);
        }
    }
    for (int key = lowest; key <= highest; ++key) {
        if (!isBlack(key)) continue;
        const auto r = keyBounds(key);
        g.setColour(on(key) ? colours::blood : colours::charcoal);
        g.fillRect(r);
        g.setColour(colours::ink);
        g.drawRect(r, 1.0f);
    }
}

void InstrumentPanel::Keyboard::press(int key, float velocity) {
    if (key == held) return;
    if (held >= 0) processor.keyboardNote(held, 0.0f);
    held = key;
    if (key >= 0) processor.keyboardNote(key, velocity);
}

void InstrumentPanel::Keyboard::mouseDown(const juce::MouseEvent& e) {
    const int key = keyAt(e.position);
    if (key < 0) return;
    // Lower on the key: louder.
    const auto r = keyBounds(key);
    press(key, juce::jlimit(0.25f, 1.0f, 0.3f + 0.7f * (e.position.y - r.getY()) / r.getHeight()));
}

void InstrumentPanel::Keyboard::mouseDrag(const juce::MouseEvent& e) {
    const int key = keyAt(e.position);
    if (key >= 0 && key != held) press(key, 0.75f);
}

void InstrumentPanel::Keyboard::mouseUp(const juce::MouseEvent&) { press(-1, 0.0f); }
}
