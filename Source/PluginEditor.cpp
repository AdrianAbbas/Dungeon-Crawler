#include "PluginEditor.h"

using namespace dc::ui;

DungeonCrawlerEditor::DungeonCrawlerEditor(DungeonCrawlerProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&look);
    setOpaque(true);

    auto* locationParameter = processor.parameters.getParameter(dc::ids::location);
    for (int i = 0; i < dc::LocationCount; ++i) {
        auto& button = places[static_cast<std::size_t>(i)];
        button = std::make_unique<juce::TextButton>(dc::locationNames[static_cast<std::size_t>(i)]);
        button->setClickingTogglesState(false);
        button->onClick = [this, i] { locationAttachment->setValueAsCompleteGesture(static_cast<float>(i)); };
        addAndMakeVisible(*button);
    }
    locationAttachment = std::make_unique<juce::ParameterAttachment>(*locationParameter,
        [this](float value) { showLocation(juce::roundToInt(value)); });
    locationAttachment->sendInitialUpdate();

    placeName.setJustificationType(juce::Justification::centred);
    placeName.setFont(titleFont(30.0f));
    placeName.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(placeName);

    setUpFader(input, dc::ids::input, "Input");
    setUpFader(intensity, dc::ids::intensity, "Intensity");
    setUpFader(mix, dc::ids::mix, "Mix");
    setUpFader(output, dc::ids::output, "Output");
    bypass.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::ButtonParameterAttachment>(*processor.parameters.getParameter(dc::ids::bypass), bypass);
    addAndMakeVisible(bypass);

    setSize(defaultWidth, defaultHeight);
}

DungeonCrawlerEditor::~DungeonCrawlerEditor() { setLookAndFeel(nullptr); }

void DungeonCrawlerEditor::setUpFader(Fader& fader, const char* id, const juce::String& name) {
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
    placeName.setText(dc::locationNames[static_cast<std::size_t>(index)], juce::dontSendNotification);
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
}

void DungeonCrawlerEditor::paint(juce::Graphics& g) {
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight()) renderBackground();
    g.drawImageAt(background, 0, 0);
}

void DungeonCrawlerEditor::resized() {
    renderBackground();
    auto area = getLocalBounds().reduced(30).withTrimmedTop(76);
    bypass.setBounds(getWidth() - 130, 30, 100, 30);
    const int faderWidth = 70;
    auto leftColumn = area.removeFromLeft(faderWidth * 2);
    auto rightColumn = area.removeFromRight(faderWidth * 2);
    const auto place = [](Fader& f, juce::Rectangle<int> r) {
        f.label.setBounds(r.removeFromBottom(22));
        f.slider.setBounds(r.reduced(4, 0));
    };
    place(input, leftColumn.removeFromLeft(faderWidth));
    place(intensity, leftColumn);
    place(mix, rightColumn.removeFromLeft(faderWidth));
    place(output, rightColumn);
    area.reduce(20, 0);
    placeName.setBounds(area.removeFromTop(60));
    area.removeFromTop(10);
    const int rows = 5, columns = 2;
    const int cellW = area.getWidth() / columns, cellH = juce::jmin(52, area.getHeight() / rows);
    for (int i = 0; i < dc::LocationCount; ++i) {
        const int column = i / rows, row = i % rows;
        places[static_cast<std::size_t>(i)]->setBounds(area.getX() + column * cellW + 6, area.getY() + row * cellH + 4, cellW - 12, cellH - 8);
    }
}
