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
// The panel's own tones: iron-dark stone, a slightly warmer band at the top.
const juce::Colour stoneTop { 0xff1d1a20 }, stoneBottom { 0xff0d0c10 }, boneKey { 0xffcdc4b2 }, boneKeyShade { 0xffa79e8d };
}

InstrumentPanel::InstrumentPanel(DungeonCrawlerProcessor& p) : processor(p), dungeon(p.parameters), keyboard(p) {
    setOpaque(true);
    setLookAndFeel(&look);
    place.setFont(labelFont(13.5f));
    place.setColour(juce::Label::textColourId, colours::ash);
    place.setMinimumHorizontalScale(0.7f);
    name.setFont(titleFont(19.0f));
    name.setColour(juce::Label::textColourId, colours::ivory);
    name.setMinimumHorizontalScale(0.7f);
    status.setFont(labelFont(13.0f));
    status.setColour(juce::Label::textColourId, colours::ash);
    status.setJustificationType(juce::Justification::topLeft);
    for (auto* l : { &place, &name, &status }) {
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*l);
    }
    previous.setButtonText(juce::String::fromUTF8("\xe2\x80\xb9"));
    next.setButtonText(juce::String::fromUTF8("\xe2\x80\xba"));
    previous.setTooltip("Previous instrument (plays it here, locked)");
    next.setTooltip("Next instrument (plays it here, locked)");
    previous.onClick = [this] { step(-1); };
    next.onClick = [this] { step(1); };
    addAndMakeVisible(previous);
    addAndMakeVisible(next);

    // The chooser: every instrument, under its location. Choosing one locks it.
    for (int l = 0; l < dc::LocationCount; ++l) {
        choose.addSectionHeading(dc::locationNames[std::size_t(l)]);
        for (int k = 0; k <= dc::subLocationCount(l); ++k) {
            const int index = dc::inst::designFor(l, k);
            choose.addItem(juce::String(dc::inst::design(index).name) + "  (" + dc::subLocationName(l, k) + ")", index + 1);
        }
    }
    choose.setTextWhenNothingSelected("Instruments");
    choose.setTooltip("Play any place's instrument here (locks it)");
    choose.onChange = [this] {
        const int index = choose.getSelectedId() - 1;
        if (index >= 0 && index != processor.instrumentIndex()) processor.chooseInstrument(index);
        refresh();
    };
    addAndMakeVisible(choose);

    lock.setTooltip("Keep this instrument while you travel; the space still changes");
    lock.setClickingTogglesState(true);
    lock.onClick = [this] {
        // Locking keeps what plays now.
        processor.setInstrumentLock(lock.getToggleState());
        refresh();
    };
    addAndMakeVisible(lock);

    bendRangeLabel.setText("Bend", juce::dontSendNotification);
    bendRangeLabel.setFont(labelFont(13.5f));
    bendRangeLabel.setJustificationType(juce::Justification::centredRight);
    bendRangeLabel.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(bendRangeLabel);
    for (int st = 1; st <= 24; ++st) bendRange.addItem(juce::String::fromUTF8("\xc2\xb1") + juce::String(st), st);
    bendRange.setTooltip("Pitch-bend range (semitones)");
    if (auto* parameter = processor.parameters.getParameter(dc::ids::bendRange))
        bendAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*parameter, bendRange);
    addAndMakeVisible(bendRange);

    // The sliders: the five macros (named by the instrument), Vowel, Ensemble, Level, Intensity.
    const std::array<const char*, sliderCount> ids { dc::ids::instMacros[0], dc::ids::instMacros[1], dc::ids::instMacros[2], dc::ids::instMacros[3],
                                                     dc::ids::instMacros[4], dc::ids::instVowel, dc::ids::instEnsemble, dc::ids::instLevel, dc::ids::intensity,
                                                     dc::ids::instPunch, dc::ids::instPunchTime };
    const std::array<const char*, sliderCount> names { "Macro", "Macro", "Macro", "Macro", "Macro", "Vowel", "Ensemble", "Level", "Intensity", "Punch", "Punch Time" };
    for (std::size_t k = 0; k < sliders.size(); ++k) {
        auto& s = sliders[k];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 56, 15);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxTextColourId, colours::ash);
        addAndMakeVisible(s);
        auto& l = macroLabels[k];
        l.setText(names[k], juce::dontSendNotification);
        l.setFont(labelFont(13.5f));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, colours::ivory.withAlpha(0.92f));
        l.setMinimumHorizontalScale(0.75f);
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
        if (auto* parameter = processor.parameters.getParameter(ids[k])) {
            s.setDoubleClickReturnValue(true, double(parameter->convertFrom0to1(parameter->getDefaultValue())));
            attachments[k] = std::make_unique<juce::SliderParameterAttachment>(*parameter, s);
        }
    }
    sliders[vowelSlot].setTooltip("The voices' vowel, from the first to the second (50 %: the instrument's own)");
    sliders[ensembleSlot].setTooltip("The voices: one singer at 0 %, the instrument's choir at 50 %, a full choir at 100 %");
    sliders[levelSlot].setTooltip("The instrument's level into its place");
    sliders[intensitySlot].setTooltip("How strongly the place's effects apply");
    // Session 12: Punch and Punch Time as in LIGHT SEEKER: their own names, the readouts in
    // dB on the hit (for the instrument playing; "Off" at none) and in ms, bright, and typed
    // into (a value, or "Off"); double-click for the default.
    sliders[punchSlot].setTooltip("Gain on hit: each note starts louder and settles to its level (the middle: the instrument's own; up to +12 dB, +18 dB on the brass)");
    sliders[punchTimeSlot].setTooltip("How long the Punch takes to settle (20-500 ms)");
    for (std::size_t k : { punchSlot, punchTimeSlot }) {
        auto& sl = sliders[k];
        sl.setTextBoxStyle(juce::Slider::TextBoxBelow, false, punchWidth, 16);
        sl.setColour(juce::Slider::textBoxTextColourId, colours::ivory);
        sl.setColour(juce::Slider::textBoxHighlightColourId, colours::blood.withAlpha(0.45f));
        macroLabels[k].setColour(juce::Label::textColourId, colours::ivory);
    }
    mix.setClickingTogglesState(true);
    mix.setTooltip("Mix: how much reverb, echoes, modulation, character and atmosphere; the reverbs' helpers");
    mix.onClick = [this] { if (onMix) onMix(); };
    addAndMakeVisible(mix);

    // The Effects roller: the panel's most prominent control.
    if (auto* parameter = processor.parameters.getParameter(dc::ids::effects)) effects.attach(*parameter);
    effects.setTooltip("Effects: how much of the place's effects (0 %: nearly dry, 100 %: as designed, 150 %: stronger)");
    addAndMakeVisible(effects);
    addAndMakeVisible(dungeon);   // Session 15

    // The wheels: bend springs back to the middle; mod stays where it is left.
    for (auto* w : { &bend, &mod }) {
        w->showLabel = false;
        w->showReadout = false;
        addAndMakeVisible(*w);
    }
    bend.setRange(-1.0, 1.0);
    bend.setValue(0.0, juce::dontSendNotification);
    bend.setDoubleClickReturnValue(true, 0.0);
    bend.notchValue = 0.0;
    bend.fillFrom = 0.0;
    mod.setRange(0.0, 1.0);
    mod.setValue(0.0, juce::dontSendNotification);
    mod.showNotch = false;
    bend.onDragEnd = [this] { bend.setValue(0.0); };
    bend.onValueChange = [this] { processor.panelBend(static_cast<float>(bend.getValue())); };
    mod.onValueChange = [this] { processor.panelMod(static_cast<float>(mod.getValue())); };
    bend.setTooltip("Pitch bend");
    for (auto* l : { &bendLabel, &modLabel }) {
        l->setFont(labelFont(12.5f));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, colours::ash);
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*l);
    }
    bendLabel.setText("Bend", juce::dontSendNotification);
    modLabel.setText("Mod", juce::dontSendNotification);
    sustain.setClickingTogglesState(true);
    sustain.setTooltip("The sustain pedal");
    sustain.onClick = [this] { processor.panelSustain(sustain.getToggleState()); };
    addAndMakeVisible(sustain);
    addAndMakeVisible(keyboard);
    addMouseListener(this, true);   // stale hovers swept on any mouse event in the panel
    refresh();
}

InstrumentPanel::~InstrumentPanel() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void InstrumentPanel::step(int direction) {
    const int index = (processor.instrumentIndex() + direction + dc::inst::instrumentCount) % dc::inst::instrumentCount;
    processor.chooseInstrument(index);
    refresh();
}

void InstrumentPanel::refresh() {
    // The instrument as the parameters say (the locked one, or the place's): at once, also
    // before the audio thread has played a block with it.
    const int index = processor.isInstrument() ? processor.instrumentIndex() : 0;
    const auto& d = dc::inst::design(index);
    const int l = dc::inst::locationOf(index), k = dc::inst::subOf(index);
    shownInstrument = index;
    shownPlaceCode = processor.currentDestination.load() * 8 + processor.currentSubLocation.load();
    shownLock = processor.instrumentLocked();
    shownTitle = d.name;
    shownPlace = juce::String(dc::locationNames[std::size_t(l)]) + juce::String::fromUTF8(" \xc2\xb7 ") + dc::subLocationName(l, k);
    const int here = processor.currentDestination.load(), hereSub = processor.currentSubLocation.load();
    const bool elsewhere = shownLock && dc::inst::designFor(here, hereSub) != index;
    name.setText(shownTitle, juce::dontSendNotification);
    place.setText(shownPlace, juce::dontSendNotification);
    if (elsewhere) shownPlace << juce::String::fromUTF8(" \xe2\x80\x94 played in ") << dc::subLocationName(here, hereSub);
    juce::String text;
    text << "Mod wheel:\n" << d.modLabel;
    if (elsewhere) text << "\n\nLocked, played in\n" << dc::subLocationName(here, hereSub);
    else if (shownLock) text << "\n\nLocked";
    status.setText(text, juce::dontSendNotification);
    static constexpr const char* tips[dc::inst::macroCount] { "Brighter or darker", "How it starts: softer or harder", "How long it rings after the key",
                                                              "The instrument's body", "The instrument's own character" };
    for (std::size_t m = 0; m < std::size_t(dc::inst::macroCount); ++m) {
        macroLabels[m].setText(d.macros[m], juce::dontSendNotification);
        sliders[m].setTooltip(juce::String(d.macros[m]) + ": " + tips[m]);
    }
    // Vowel and Ensemble only act on the instruments that sing (dimmed elsewhere).
    const bool voiced = processor.isInstrument() && dc::inst::designUsesVoice(index);
    for (std::size_t s : { vowelSlot, ensembleSlot }) {
        sliders[s].setEnabled(voiced);
        sliders[s].setAlpha(voiced ? 1.0f : 0.35f);
        macroLabels[s].setAlpha(voiced ? 1.0f : 0.35f);
    }
    choose.setSelectedId(index + 1, juce::dontSendNotification);
    lock.setToggleState(shownLock, juce::dontSendNotification);
    sliders[punchSlot].updateText();   // the Punch reads in dB of this instrument's own (LIGHT SEEKER)
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
    const int code = processor.currentDestination.load() * 8 + processor.currentSubLocation.load();
    if (processor.instrumentIndex() != shownInstrument || processor.instrumentLocked() != shownLock || code != shownPlaceCode) refresh();
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
    // The wheels and the pedal as the audio thread last saw them (the host's or the panel's).
    const float b = processor.shownBend.load(), m = processor.shownMod.load();
    const bool pedal = processor.shownSustain.load();
    if (std::abs(b - shownBend) > 0.01f) {
        shownBend = b;
        if (!bend.isMouseButtonDown()) { bend.setValue(b, juce::dontSendNotification); ++repaintRequests; }
    }
    if (std::abs(m - shownMod) > 0.01f) {
        shownMod = m;
        if (!mod.isMouseButtonDown()) { mod.setValue(m, juce::dontSendNotification); ++repaintRequests; }
    }
    if (pedal != shownPedal) {
        shownPedal = pedal;
        if (!sustain.isMouseButtonDown()) { sustain.setToggleState(pedal, juce::dontSendNotification); ++repaintRequests; }
    }
}

void InstrumentPanel::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    // Dark stone: a vertical gradient, an iron rail with a crimson edge on top.
    g.setGradientFill(juce::ColourGradient(stoneTop, 0.0f, 0.0f, stoneBottom, 0.0f, area.getBottom(), false));
    g.fillAll();
    g.setColour(juce::Colours::black);
    g.fillRect(0.0f, 0.0f, area.getWidth(), 3.0f);
    g.setColour(colours::blood.withAlpha(0.75f));
    g.fillRect(0.0f, 3.0f, area.getWidth(), 1.5f);
    g.setColour(colours::dust.withAlpha(0.45f));
    g.fillRect(262.0f, 12.0f, 1.0f, 90.0f);    // instrument | sliders
    g.fillRect(808.0f, 12.0f, 1.0f, 90.0f);    // sliders | Effects
    g.fillRect(12.0f, 109.0f, area.getWidth() - 24.0f, 1.0f);   // the keyboard row
    g.fillRect(area.getWidth() - 108.0f, 116.0f, 1.0f, 70.0f);   // keyboard | DUNGEON SYNTH (Session 15)
    // A faint crimson glow behind the roller: the panel's main control.
    const auto glow = effects.getBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::blood.withAlpha(0.13f), glow.getCentre(), colours::blood.withAlpha(0.0f),
                                           glow.getCentre().translated(glow.getWidth() * 0.9f, 0.0f), true));
    g.fillRect(glow.expanded(14.0f, 2.0f));
    g.setColour(colours::ash);
    g.setFont(titleFont(10.5f));
    g.drawText("INSTRUMENT", 274, 9, 200, 14, juce::Justification::centredLeft);
}

void InstrumentPanel::resized() {
    // Top band: the instrument (0-262), its sliders (262-808), the Effects roller and what
    // the wheel does (808-); bottom band: the wheels and the pedal (0-150), the keyboard.
    place.setBounds(12, 8, 246, 16);
    name.setBounds(10, 24, 248, 26);
    previous.setBounds(10, 54, 28, 24);
    choose.setBounds(42, 54, 182, 24);
    next.setBounds(228, 54, 28, 24);
    lock.setBounds(10, 82, 140, 24);
    bendRangeLabel.setBounds(150, 82, 42, 24);
    bendRange.setBounds(196, 82, 60, 24);
    // Eleven sliders (Session 11: Punch and its Time after Intensity), grouped by gaps.
    // Session 12: Punch and Punch Time in wider slots (LIGHT SEEKER's readouts, "+12.0 dB").
    int x = 270;
    for (std::size_t k = 0; k < sliders.size(); ++k) {
        const bool punch = k >= punchSlot;
        const int w = punch ? punchWidth : 40;
        macroLabels[k].setBounds(x + w / 2 - (punch ? 34 : 31), 24, punch ? 68 : 62, 16);
        sliders[k].setBounds(x, 40, w, 66);
        x += (k == 4 || k == 6 || k == 8) ? 51 : punch ? punchWidth + 4 : 44;
    }
    effects.setBounds(816, 8, 62, 98);
    status.setBounds(884, 22, getWidth() - 888, 58);
    mix.setBounds(884, 82, getWidth() - 892, 24);
    bendLabel.setBounds(8, 113, 34, 14);
    modLabel.setBounds(44, 113, 34, 14);
    bend.setBounds(10, 127, 30, 62);
    mod.setBounds(46, 127, 30, 62);
    sustain.setBounds(84, 150, 62, 30);
    keyboard.setBounds(154, 115, getWidth() - 162 - 104, getHeight() - 121);
    dungeon.setBounds(getWidth() - 102, 111, 96, getHeight() - 115);
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
    g.fillAll(colours::ink);
    const auto on = [this](int key) { return ((lit[std::size_t(key >> 6)] >> (key & 63)) & 1u) != 0; };
    for (int key = lowest; key <= highest; ++key) {
        if (isBlack(key)) continue;
        const auto r = keyBounds(key).reduced(0.5f, 0.0f);
        // Aged bone keys, shaded towards the front; a lit key glows crimson.
        if (on(key)) g.setGradientFill(juce::ColourGradient(colours::ember, 0.0f, r.getY(), colours::blood.darker(0.3f), 0.0f, r.getBottom(), false));
        else g.setGradientFill(juce::ColourGradient(boneKey, 0.0f, r.getY(), boneKeyShade, 0.0f, r.getBottom(), false));
        g.fillRect(r);
        g.setColour(colours::ink);
        g.drawRect(r, 1.0f);
        if (key % 12 == 0) {
            g.setColour(on(key) ? colours::ivory : juce::Colour(0xff4a4550));
            g.setFont(labelFont(11.0f));
            g.drawText("C" + juce::String(key / 12 - 2), r.withTrimmedTop(r.getHeight() - 16.0f).toNearestInt(), juce::Justification::centred);
        }
    }
    for (int key = lowest; key <= highest; ++key) {
        if (!isBlack(key)) continue;
        const auto r = keyBounds(key);
        // Iron keys with a faint top light.
        if (on(key)) g.setGradientFill(juce::ColourGradient(colours::ember, 0.0f, r.getY(), colours::blood.darker(0.4f), 0.0f, r.getBottom(), false));
        else g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2c2932), 0.0f, r.getY(), juce::Colour(0xff111014), 0.0f, r.getBottom(), false));
        g.fillRect(r);
        g.setColour(colours::ink);
        g.drawRect(r, 1.0f);
    }
    // The rail's shadow over the keys' tops.
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.45f), 0.0f, 0.0f, juce::Colours::black.withAlpha(0.0f), 0.0f, 6.0f, false));
    g.fillRect(0, 0, getWidth(), 6);
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
