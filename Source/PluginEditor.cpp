#include "PluginEditor.h"

using namespace dc::ui;

namespace {
constexpr const char* moduleNames[] = { "I. The Abyss", "II. Ancient Blood", "III. The Choir", "IV. The Wraith",
                                        "V. The Echoes", "VI. The Crypt", "VII. Relics" };
// The first module parameter of each module (Frame order).
constexpr dc::P moduleStarts[] = { dc::Cutoff, dc::Drive, dc::Voices, dc::Blur, dc::DelayTime, dc::Space, dc::Flicker };
int moduleOf(std::size_t index) {
    int group = 0;
    for (int g = 0; g < 7; ++g) if (index >= static_cast<std::size_t>(moduleStarts[g])) group = g;
    return group;
}
}

//==============================================================================
DungeonCrawlerEditor::Drawer::Drawer(juce::AudioProcessorValueTreeState& state) {
    setOpaque(true);
    hint.setText("Advanced: every module parameter, as an offset from the place (0 % = as designed) or an override "
                 "(\"Place\" = as designed). Offsets travel with you.", juce::dontSendNotification);
    hint.setFont(labelFont(13.0f));
    hint.setColour(juce::Label::textColourId, colours::ash);
    addAndMakeVisible(hint);
    int group = -1;
    for (std::size_t i = 0; i < dc::Count; ++i) {
        if (moduleOf(i) != group) {
            group = moduleOf(i);
            auto heading = std::make_unique<juce::Label>();
            heading->setText(moduleNames[group], juce::dontSendNotification);
            heading->setFont(titleFont(15.0f));
            heading->setColour(juce::Label::textColourId, colours::blood.brighter(0.3f));
            content.addAndMakeVisible(*heading);
            headings.push_back(std::move(heading));
        }
        auto row = std::make_unique<Row>();
        row->group = group;
        row->label.setText(dc::moduleSpecs[i].name, juce::dontSendNotification);
        row->label.setFont(labelFont(13.0f));
        content.addAndMakeVisible(row->label);
        auto* parameter = state.getParameter(dc::ids::advanced(i));
        if (dc::moduleSpecs[i].glide == dc::Glide::Step) {
            row->box = std::make_unique<juce::ComboBox>();
            row->box->addItemList(dc::advancedChoices(dc::moduleSpecs[i]), 1);
            row->boxAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*parameter, *row->box);
            content.addAndMakeVisible(*row->box);
        } else {
            row->slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row->slider->setTextBoxStyle(juce::Slider::TextBoxRight, true, 56, 18);
            row->slider->setDoubleClickReturnValue(true, 0.0);
            row->slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            row->sliderAttachment = std::make_unique<juce::SliderParameterAttachment>(*parameter, *row->slider);
            content.addAndMakeVisible(*row->slider);
        }
        rows.push_back(std::move(row));
    }
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void DungeonCrawlerEditor::Drawer::paint(juce::Graphics& g) {
    g.fillAll(colours::ink);
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawRect(getLocalBounds(), 1);
}

void DungeonCrawlerEditor::Drawer::resized() {
    auto area = getLocalBounds().reduced(10);
    hint.setBounds(area.removeFromTop(34));
    viewport.setBounds(area);
    // Two columns of rows, headings spanning their column.
    const int width = viewport.getWidth() - viewport.getScrollBarThickness() - 4;
    const int columnWidth = width / 2, rowHeight = 24, headingHeight = 26;
    std::array<int, 2> y { 0, 0 };
    int headingIndex = -1, group = -1, column = 0;
    for (auto& row : rows) {
        if (row->group != group) {
            group = row->group;
            ++headingIndex;
            column = y[0] <= y[1] ? 0 : 1;
            headings[static_cast<std::size_t>(headingIndex)]->setBounds(column * columnWidth, y[static_cast<std::size_t>(column)], columnWidth - 8, headingHeight);
            y[static_cast<std::size_t>(column)] += headingHeight;
        }
        auto r = juce::Rectangle<int>(column * columnWidth, y[static_cast<std::size_t>(column)], columnWidth - 12, rowHeight);
        row->label.setBounds(r.removeFromLeft(120));
        if (row->slider) row->slider->setBounds(r.reduced(0, 2));
        if (row->box) row->box->setBounds(r.reduced(0, 2).withWidth(juce::jmin(r.getWidth(), 170)));
        y[static_cast<std::size_t>(column)] += rowHeight;
    }
    content.setSize(width, juce::jmax(y[0], y[1]) + 8);
}

//==============================================================================
DungeonCrawlerEditor::DungeonCrawlerEditor(DungeonCrawlerProcessor& p)
    : AudioProcessorEditor(p), processor(p), drawer(p.parameters) {
    setLookAndFeel(&look);
    setOpaque(true);

    auto* locationParameter = processor.parameters.getParameter(dc::ids::location);
    auto* subParameter = processor.parameters.getParameter(dc::ids::subLocation);
    for (int i = 0; i < dc::LocationCount; ++i) {
        auto& button = places[static_cast<std::size_t>(i)];
        button = std::make_unique<juce::TextButton>(dc::locationNames[static_cast<std::size_t>(i)]);
        button->setClickingTogglesState(false);
        // Choosing a location arrives at its own scene (sub-location 0).
        button->onClick = [this, i] {
            subLocationAttachment->setValueAsCompleteGesture(0.0f);
            locationAttachment->setValueAsCompleteGesture(static_cast<float>(i));
        };
        addAndMakeVisible(*button);
    }
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = subs[static_cast<std::size_t>(k)];
        button = std::make_unique<juce::TextButton>();
        button->setClickingTogglesState(false);
        button->onClick = [this, k] { subLocationAttachment->setValueAsCompleteGesture(static_cast<float>(k)); };
        addChildComponent(*button);
    }
    locationAttachment = std::make_unique<juce::ParameterAttachment>(*locationParameter, [this](float value) { showLocation(juce::roundToInt(value)); });
    subLocationAttachment = std::make_unique<juce::ParameterAttachment>(*subParameter, [this](float value) { showSubLocation(juce::roundToInt(value)); });

    placeName.setJustificationType(juce::Justification::centredLeft);
    placeName.setFont(titleFont(26.0f));
    placeName.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(placeName);
    macroHeading.setText("character", juce::dontSendNotification);
    macroHeading.setFont(labelFont(13.0f));
    macroHeading.setColour(juce::Label::textColourId, colours::ash);
    addAndMakeVisible(macroHeading);

    for (std::size_t m = 0; m < macros.size(); ++m) setUpFader(macros[m], dc::ids::macros[m], "Macro " + juce::String(m + 1));
    setUpFader(input, dc::ids::input, "Input");
    setUpFader(intensity, dc::ids::intensity, "Intensity");
    setUpFader(mix, dc::ids::mix, "Mix");
    setUpFader(output, dc::ids::output, "Output");

    travelLabel.setText("Travel", juce::dontSendNotification);
    travelLabel.setFont(labelFont(14.0f));
    addAndMakeVisible(travelLabel);
    travelTime.setTextBoxStyle(juce::Slider::TextBoxRight, true, 62, 18);
    travelTime.setDoubleClickReturnValue(true, 1.5);
    travelTime.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    travelTimeAttachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(dc::ids::travelTime), travelTime);
    addAndMakeVisible(travelTime);
    travelSync.addItemList(juce::StringArray::fromTokens(dc::travelSyncChoices, "|", ""), 1);
    travelSyncAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*processor.parameters.getParameter(dc::ids::travelSync), travelSync);
    addAndMakeVisible(travelSync);

    bypass.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::ButtonParameterAttachment>(*processor.parameters.getParameter(dc::ids::bypass), bypass);
    addAndMakeVisible(bypass);
    advancedToggle.setClickingTogglesState(true);
    advancedToggle.onClick = [this] { drawer.setVisible(advancedToggle.getToggleState()); };
    addAndMakeVisible(advancedToggle);
    addChildComponent(drawer);

    locationAttachment->sendInitialUpdate();
    subLocationAttachment->sendInitialUpdate();
    setSize(defaultWidth, defaultHeight);
}

DungeonCrawlerEditor::~DungeonCrawlerEditor() { setLookAndFeel(nullptr); }

void DungeonCrawlerEditor::setUpFader(Fader& fader, const juce::String& id, const juce::String& name) {
    fader.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 64, 18);
    fader.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    fader.slider.setDoubleClickReturnValue(true, static_cast<double>(
        processor.parameters.getParameter(id)->convertFrom0to1(processor.parameters.getParameter(id)->getDefaultValue())));
    fader.attachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(id), fader.slider);
    fader.label.setText(name, juce::dontSendNotification);
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setFont(labelFont(14.0f));
    addAndMakeVisible(fader.slider);
    addAndMakeVisible(fader.label);
}

void DungeonCrawlerEditor::showLocation(int index) {
    index = juce::jlimit(0, dc::LocationCount - 1, index);
    shown = index;
    for (int i = 0; i < dc::LocationCount; ++i)
        places[static_cast<std::size_t>(i)]->setToggleState(i == index, juce::dontSendNotification);
    const int count = dc::subLocationCount(index);
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = *subs[static_cast<std::size_t>(k)];
        button.setVisible(k <= count);
        button.setButtonText(k <= count ? dc::subLocationName(index, k) : "");
    }
    // Only the location's own macros are shown, under their themed names.
    for (int m = 0; m < dc::macroCount; ++m) {
        auto& fader = macros[static_cast<std::size_t>(m)];
        const bool used = m < dc::macroCountFor(index);
        fader.slider.setVisible(used);
        fader.label.setVisible(used);
        fader.label.setText(dc::macroName(index, m), juce::dontSendNotification);
    }
    showSubLocation(shownSub < 0 ? 0 : shownSub);
}

void DungeonCrawlerEditor::showSubLocation(int index) {
    if (shown < 0) { shownSub = index; return; }
    shownSub = juce::jlimit(0, dc::maxSubLocations, index);
    const int effective = dc::clampSubLocation(shown, shownSub);
    for (int k = 0; k < dc::placesPerLocation; ++k)
        subs[static_cast<std::size_t>(k)]->setToggleState(k == effective, juce::dontSendNotification);
    placeName.setText(juce::String(dc::locationNames[static_cast<std::size_t>(shown)]) + juce::String::fromUTF8("  \xe2\x80\xba  ")
                      + dc::subLocationName(shown, effective), juce::dontSendNotification);
}

void DungeonCrawlerEditor::renderBackground() {
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    background = juce::Image(juce::Image::RGB, w, h, true);
    juce::Graphics g(background);
    g.fillAll(colours::ink);
    // Engraver's hatching: thin diagonal strokes whose density follows a soft light
    // from the top centre (deterministic, drawn once per size).
    juce::Random random(0x44475231);
    for (int k = -h; k < w; k += 3) {
        const float jitter = random.nextFloat() * 1.5f;
        const float x0 = static_cast<float>(k) + jitter;
        const float shade = 0.025f + 0.035f * random.nextFloat();
        g.setColour(colours::ivory.withAlpha(shade));
        g.drawLine(x0, 0.0f, x0 + static_cast<float>(h) * 0.6f, static_cast<float>(h), 0.6f);
    }
    juce::ColourGradient vignette(juce::Colours::transparentBlack, static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.35f,
                                  juce::Colours::black.withAlpha(0.75f), 0.0f, static_cast<float>(h), true);
    g.setGradientFill(vignette);
    g.fillAll();
    // Engraved plate border: a double rule.
    const auto frame = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(colours::ash.withAlpha(0.5f));
    g.drawRect(frame, 1.0f);
    g.drawRect(frame.reduced(4.0f), 0.6f);
    // Title with the red initial.
    const auto titleArea = juce::Rectangle<float>(30.0f, 22.0f, static_cast<float>(w) - 60.0f, 50.0f);
    juce::AttributedString title;
    title.append("D", titleFont(44.0f), colours::blood);
    title.append("UNGEON CRAWLER", titleFont(36.0f), colours::ivory);
    title.setJustification(juce::Justification::centredLeft);
    title.draw(g, titleArea);
    g.setColour(colours::ash.withAlpha(0.7f));
    g.setFont(labelFont(13.0f));
    g.drawText("choose a place to travel to", juce::Rectangle<float>(30.0f, 70.0f, 400.0f, 18.0f), juce::Justification::centredLeft);
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawHorizontalLine(96, 30.0f, static_cast<float>(w) - 30.0f);
    // Column rules.
    g.drawVerticalLine(232, 110.0f, static_cast<float>(h) - 30.0f);
    g.drawVerticalLine(640, 110.0f, static_cast<float>(h) - 30.0f);
}

void DungeonCrawlerEditor::paint(juce::Graphics& g) {
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight()) renderBackground();
    g.drawImageAt(background, 0, 0);
}

void DungeonCrawlerEditor::resized() {
    renderBackground();
    bypass.setBounds(getWidth() - 130, 30, 100, 30);
    advancedToggle.setBounds(getWidth() - 240, 30, 104, 30);
    const int top = 108, bottom = getHeight() - 32;
    // Left: the locations.
    const int rowHeight = (bottom - top) / dc::LocationCount;
    for (int i = 0; i < dc::LocationCount; ++i)
        places[static_cast<std::size_t>(i)]->setBounds(26, top + i * rowHeight, 196, rowHeight);
    // Centre: the place, its sub-locations and the macros.
    placeName.setBounds(246, top, 390, 36);
    for (int k = 0; k < dc::placesPerLocation; ++k)
        subs[static_cast<std::size_t>(k)]->setBounds(246, top + 46 + k * 38, 170, 36);
    macroHeading.setBounds(430, top + 40, 200, 18);
    const int faderWidth = 52;
    const auto place = [](Fader& f, juce::Rectangle<int> r) {
        f.label.setBounds(r.removeFromBottom(22));
        f.slider.setBounds(r.reduced(2, 0));
    };
    for (int m = 0; m < dc::macroCount; ++m)
        place(macros[static_cast<std::size_t>(m)], { 426 + m * faderWidth, top + 60, faderWidth, bottom - top - 60 });
    // Right: the main faders and the travel controls.
    const int rightX = 652, mainWidth = 68;
    place(input, { rightX, top, mainWidth, bottom - top - 70 });
    place(intensity, { rightX + mainWidth, top, mainWidth, bottom - top - 70 });
    place(mix, { rightX + 2 * mainWidth, top, mainWidth, bottom - top - 70 });
    place(output, { rightX + 3 * mainWidth, top, mainWidth, bottom - top - 70 });
    travelLabel.setBounds(rightX, bottom - 60, 60, 24);
    travelTime.setBounds(rightX + 56, bottom - 60, 216, 24);
    travelSync.setBounds(rightX + 56, bottom - 28, 120, 24);
    drawer.setBounds(240, top - 4, getWidth() - 266, bottom - top + 8);
}
