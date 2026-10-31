#include "PluginEditor.h"

using namespace dc::ui;

namespace {
constexpr const char* moduleNames[] = { "I. The Abyss", "II. Ancient Blood", "III. The Choir", "IV. The Wraith",
                                        "V. The Echoes", "VI. The Crypt", "VII. Relics", "VIII. Signature" };
// The first module parameter of each module (Frame order).
constexpr dc::P moduleStarts[] = { dc::Cutoff, dc::Drive, dc::Voices, dc::Blur, dc::DelayTime, dc::Space, dc::Flicker, dc::SigKind };
int moduleOf(std::size_t index) {
    int group = 0;
    for (int g = 0; g < 8; ++g) if (index >= static_cast<std::size_t>(moduleStarts[g])) group = g;
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
namespace {
constexpr float stripHeight = 112.0f;
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
float ease(float t) { t = juce::jlimit(0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }
}

void DungeonCrawlerEditor::LandmarkButton::paintButton(juce::Graphics& g, bool highlighted, bool) {
    if (!highlighted) return;
    const auto r = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(colours::ivory.withAlpha(0.18f));
    g.fillEllipse(r);
    g.setColour(colours::ivory.withAlpha(0.85f));
    g.drawEllipse(r, 1.4f);
}

void DungeonCrawlerEditor::HotspotButton::paintButton(juce::Graphics& g, bool highlighted, bool) {
    const auto r = getLocalBounds().toFloat();
    const auto ring = juce::Rectangle<float>(22.0f, 22.0f).withCentre({ r.getCentreX(), 14.0f });
    const bool here = getToggleState();
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(ring.expanded(3.0f));
    g.setColour(here ? colours::blood : colours::ivory.withAlpha(highlighted ? 1.0f : 0.8f));
    g.drawEllipse(ring, here ? 2.6f : 1.6f);
    g.fillEllipse(ring.reduced(7.0f));
    if (highlighted || here) {
        g.setFont(labelFont(13.0f));
        const auto text = getButtonText();
        const float width = juce::GlyphArrangement::getStringWidth(labelFont(13.0f), text) + 14.0f;
        const auto plate = juce::Rectangle<float>(width, 20.0f).withCentre({ r.getCentreX(), 38.0f });
        g.setColour(juce::Colours::black.withAlpha(0.68f));
        g.fillRoundedRectangle(plate, 3.0f);
        g.setColour(here ? colours::blood.brighter(0.4f) : colours::ivory);
        g.drawText(text, plate, juce::Justification::centred);
    }
}

DungeonCrawlerEditor::DungeonCrawlerEditor(DungeonCrawlerProcessor& p)
    : AudioProcessorEditor(p), processor(p), cache([this] { sceneReady(); }), drawer(p.parameters) {
    setLookAndFeel(&look);
    setOpaque(true);
    auto* locationParameter = processor.parameters.getParameter(dc::ids::location);
    auto* subParameter = processor.parameters.getParameter(dc::ids::subLocation);

    for (int i = 0; i < dc::LocationCount; ++i) {
        auto& button = landmarks[static_cast<std::size_t>(i)];
        button = std::make_unique<LandmarkButton>(dc::locationNames[static_cast<std::size_t>(i)]);
        button->setClickingTogglesState(false);
        button->setTooltip(dc::locationNames[static_cast<std::size_t>(i)]);
        // Choosing a location travels to its own scene (sub-location 0); the path is
        // drawn, then the view zooms in.
        button->onClick = [this, i] {
            const bool already = i == shown;
            subLocationAttachment->setValueAsCompleteGesture(0.0f);
            locationAttachment->setValueAsCompleteGesture(static_cast<float>(i));
            if (already) goTo(View::Place, { i, 0 }, 1);
            else { zoomAt = nowSeconds() + 0.55; wakeTimer(); }
        };
        button->onStateChange = [this, i] {
            const bool over = landmarks[static_cast<std::size_t>(i)]->isOver();
            if (over != (hovered == i)) { hovered = over ? i : (hovered == i ? -1 : hovered); repaint(); }
        };
        addAndMakeVisible(*button);
    }
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& spot = hotspots[static_cast<std::size_t>(k)];
        spot = std::make_unique<HotspotButton>();
        spot->onClick = [this, k] { subLocationAttachment->setValueAsCompleteGesture(static_cast<float>(k)); };
        addChildComponent(*spot);
        auto& button = subs[static_cast<std::size_t>(k)];
        button = std::make_unique<juce::TextButton>();
        button->setClickingTogglesState(false);
        button->onClick = [this, k] { subLocationAttachment->setValueAsCompleteGesture(static_cast<float>(k)); };
        addChildComponent(*button);
    }
    locationAttachment = std::make_unique<juce::ParameterAttachment>(*locationParameter, [this](float value) { showLocation(juce::roundToInt(value)); });
    subLocationAttachment = std::make_unique<juce::ParameterAttachment>(*subParameter, [this](float value) { showSubLocation(juce::roundToInt(value)); });

    // Breadcrumb: Map > Location > Place.
    mapCrumb.onClick = [this] { goTo(View::Map, { -1, 0 }, 2); };
    locationCrumb.onClick = [this] {
        if (view == View::Map) goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1);
        else subLocationAttachment->setValueAsCompleteGesture(0.0f);
    };
    placeCrumb.setFont(titleFont(18.0f));
    placeCrumb.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(mapCrumb);
    addAndMakeVisible(locationCrumb);
    addAndMakeVisible(placeCrumb);

    macroHeading.setText("character", juce::dontSendNotification);
    macroHeading.setFont(labelFont(12.0f));
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
    controlsToggle.setButtonText(juce::String::fromUTF8("\xe2\x96\xbe"));
    controlsToggle.setTooltip("Show or hide the controls");
    controlsToggle.onClick = [this] {
        controlsShown = !controlsShown;
        controlsToggle.setButtonText(juce::String::fromUTF8(controlsShown ? "\xe2\x96\xbe" : "\xe2\x96\xb4"));
        resized();
        repaint();
    };
    addAndMakeVisible(controlsToggle);
    addChildComponent(drawer);

    locationAttachment->sendInitialUpdate();
    subLocationAttachment->sendInitialUpdate();
    setSize(defaultWidth, defaultHeight);
}

DungeonCrawlerEditor::~DungeonCrawlerEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void DungeonCrawlerEditor::setUpFader(Fader& fader, const juce::String& id, const juce::String& name) {
    fader.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 16);
    fader.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    fader.slider.setDoubleClickReturnValue(true, static_cast<double>(
        processor.parameters.getParameter(id)->convertFrom0to1(processor.parameters.getParameter(id)->getDefaultValue())));
    fader.attachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(id), fader.slider);
    fader.label.setText(name, juce::dontSendNotification);
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setFont(labelFont(13.0f));
    addAndMakeVisible(fader.slider);
    addAndMakeVisible(fader.label);
}

juce::Point<int> DungeonCrawlerEditor::pixelSize() const {
    float scale = 1.0f;
    if (isOnDesktop() || getPeer() != nullptr || getParentComponent() != nullptr)
        scale = juce::jlimit(1.0f, 2.0f, static_cast<float>(juce::Component::getApproximateScaleFactorForComponent(const_cast<DungeonCrawlerEditor*>(this))));
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
        scale = juce::jlimit(1.0f, 2.0f, scale * static_cast<float>(display->scale));
    return { juce::roundToInt(static_cast<float>(getWidth()) * scale), juce::roundToInt(static_cast<float>(getHeight()) * scale) };
}

juce::Point<float> DungeonCrawlerEditor::toLocal(juce::Point<float> v) const {
    return { v.x * static_cast<float>(getWidth()) / dc::art::virtualWidth, v.y * static_cast<float>(getHeight()) / dc::art::virtualHeight };
}

juce::Rectangle<float> DungeonCrawlerEditor::controlsArea() const {
    return controlsShown ? getLocalBounds().toFloat().removeFromBottom(stripHeight) : juce::Rectangle<float>();
}

void DungeonCrawlerEditor::showLocation(int index) {
    index = juce::jlimit(0, dc::LocationCount - 1, index);
    if (shown >= 0 && index != shown) {
        // A journey on the map: remember where from, for the path.
        pathFrom = shown;
        pathTo = index;
        pathEnded = 0;
        wakeTimer();
    }
    shown = index;
    const int count = dc::subLocationCount(index);
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = *subs[static_cast<std::size_t>(k)];
        button.setButtonText(k <= count ? dc::subLocationName(index, k) : "");
        hotspots[static_cast<std::size_t>(k)]->setButtonText(k <= count ? dc::subLocationName(index, k) : "");
    }
    // Only the location's own macros are shown, under their themed names.
    for (int m = 0; m < dc::macroCount; ++m) {
        auto& fader = macros[static_cast<std::size_t>(m)];
        const bool used = m < dc::macroCountFor(index);
        fader.label.setText(dc::macroName(index, m), juce::dontSendNotification);
        fader.slider.setVisible(used && controlsShown);
        fader.label.setVisible(used && controlsShown);
    }
    showSubLocation(shownSub < 0 ? 0 : shownSub);
}

void DungeonCrawlerEditor::showSubLocation(int index) {
    if (shown < 0) { shownSub = index; return; }
    shownSub = juce::jlimit(0, dc::maxSubLocations, index);
    const int effective = dc::clampSubLocation(shown, shownSub);
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        subs[static_cast<std::size_t>(k)]->setToggleState(k == effective, juce::dontSendNotification);
        hotspots[static_cast<std::size_t>(k)]->setToggleState(k == effective, juce::dontSendNotification);
    }
    for (int i = 0; i < dc::LocationCount; ++i)
        landmarks[static_cast<std::size_t>(i)]->setToggleState(i == shown, juce::dontSendNotification);
    // Inside a location, the view follows the place (automation moves you too).
    if (view == View::Place && !(scene == dc::art::SceneId { shown, effective }))
        goTo(View::Place, { shown, effective }, 0);
    updateCrumbs();
    layoutOverlays();
    repaint();
}

void DungeonCrawlerEditor::updateCrumbs() {
    if (shown < 0) return;
    const bool map = view == View::Map;
    locationCrumb.setButtonText(dc::locationNames[static_cast<std::size_t>(shown)]);
    placeCrumb.setText(map ? juce::String() : juce::String::fromUTF8("\xe2\x80\xba  ") + dc::subLocationName(shown, dc::clampSubLocation(shown, shownSub)),
                       juce::dontSendNotification);
    mapCrumb.setToggleState(map, juce::dontSendNotification);
}

void DungeonCrawlerEditor::goTo(View target, dc::art::SceneId id, int kind) {
    if (target == View::Place) id.sub = dc::clampSubLocation(id.location, id.sub);
    pending = true;
    pendingView = target;
    pendingScene = target == View::Map ? dc::art::SceneId { -1, 0 } : id;
    pendingKind = kind;
    zoomAt = 0;
    const auto px = pixelSize();
    cache.request(pendingScene, px.x, px.y);
    wakeTimer();
}

void DungeonCrawlerEditor::wakeTimer() {
    if (!isTimerRunning()) startTimerHz(30);
}

void DungeonCrawlerEditor::sceneReady() {
    // A scene finished rendering: show it (or start the transition that waited for it).
    timerCallback();
    repaint();
}

void DungeonCrawlerEditor::timerCallback() {
    const double now = nowSeconds();
    const auto px = pixelSize();
    bool busy = false;
    if (zoomAt > 0) {
        if (now >= zoomAt) goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1);
        else busy = true;
    }
    if (pending && !transition.active) {
        busy = true;
        if (cache.ready(pendingScene, px.x, px.y)) {
            transition.active = true;
            transition.kind = pendingKind;
            transition.from = cache.get(scene, px.x, px.y);
            transition.to = cache.get(pendingScene, px.x, px.y);
            transition.start = now;
            transition.length = pendingKind == 0 ? 0.32 : 0.5;
            transition.focus = dc::art::mapLandmarks()[static_cast<std::size_t>(juce::jlimit(0, dc::LocationCount - 1,
                                   pendingView == View::Map ? scene.location : pendingScene.location))].position;
            view = pendingView;
            scene = pendingScene;
            pending = false;
            if (transition.from.isNull()) transition.active = false;   // nothing to transition from
            updateCrumbs();
            layoutOverlays();
        }
    }
    if (transition.active) {
        busy = true;
        if (now - transition.start >= transition.length) {
            transition.active = false;
            transition.from = {};
            transition.to = {};
            layoutOverlays();
        }
        repaint();
    }
    // The travel path on the map: drawn while travelling, then fades.
    if (view == View::Map && pathTo >= 0) {
        const bool travelling = processor.travelProgress.load() < 1.0f;
        if (travelling) { pathEnded = 0; busy = true; }
        else if (pathEnded <= 0) { pathEnded = now; busy = true; }
        else if (now - pathEnded < 1.5) busy = true;
        else pathTo = -1;
        repaint();
    }
    if (!busy) stopTimer();
}

void DungeonCrawlerEditor::settleForTest() {
    const auto px = pixelSize();
    if (zoomAt > 0) { zoomAt = 0; goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1); }
    if (pending) cache.renderNow(pendingScene, px.x, px.y);
    if (!cache.ready(scene, px.x, px.y)) cache.renderNow(scene, px.x, px.y);
    timerCallback();
    transition.active = false;
    transition.from = {};
    transition.to = {};
    pathTo = -1;
    layoutOverlays();
    stopTimer();
}

void DungeonCrawlerEditor::layoutOverlays() {
    const bool map = view == View::Map && !transition.active && !pending;
    const bool place = view == View::Place && !transition.active && !pending;
    for (int i = 0; i < dc::LocationCount; ++i) {
        const auto& l = dc::art::mapLandmarks()[static_cast<std::size_t>(i)];
        const auto c = toLocal(l.position);
        const float r = l.radius * static_cast<float>(getWidth()) / dc::art::virtualWidth;
        auto& button = *landmarks[static_cast<std::size_t>(i)];
        button.setBounds(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c).toNearestInt());
        button.setVisible(map);
    }
    const int count = shown >= 0 ? dc::subLocationCount(shown) : 0;
    const auto spots = shown >= 0 ? dc::art::sceneHotspots(shown, dc::clampSubLocation(shown, shownSub)) : std::vector<dc::art::Hotspot> {};
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& spot = *hotspots[static_cast<std::size_t>(k)];
        const auto found = std::find_if(spots.begin(), spots.end(), [k](const dc::art::Hotspot& h) { return h.sub == k; });
        if (found != spots.end() && k <= count) {
            const auto c = toLocal(found->position);
            spot.setBounds(juce::Rectangle<int>(160, 52).withCentre({ juce::roundToInt(c.x), juce::roundToInt(c.y) + 12 }));
            spot.setVisible(place);
        } else spot.setVisible(false);
    }
    // The compact list of places, top right.
    const int rowHeight = 26, listWidth = 172;
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = *subs[static_cast<std::size_t>(k)];
        button.setBounds(getWidth() - listWidth - 14, 16 + k * rowHeight, listWidth, rowHeight);
        button.setVisible(place && k <= count);
    }
    // Breadcrumb, top left.
    mapCrumb.setBounds(14, 14, 74, 28);
    const int locationWidth = 30 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(16.0f), locationCrumb.getButtonText()));
    locationCrumb.setBounds(mapCrumb.getRight() + 4, 14, locationWidth, 28);
    placeCrumb.setBounds(locationCrumb.getRight(), 14, 320, 28);
    locationCrumb.setVisible(!map || true);
    placeCrumb.setVisible(view == View::Place);
}

void DungeonCrawlerEditor::paintTransition(juce::Graphics& g, double now) {
    const float t = ease(static_cast<float>((now - transition.start) / transition.length));
    const auto bounds = getLocalBounds().toFloat();
    const auto focus = toLocal(transition.focus);
    const auto drawScaled = [&](const juce::Image& image, float scale, juce::Point<float> about, float alpha) {
        if (image.isNull() || alpha <= 0.0f) return;
        const auto fit = juce::AffineTransform::scale(bounds.getWidth() / static_cast<float>(image.getWidth()), bounds.getHeight() / static_cast<float>(image.getHeight()));
        g.setOpacity(alpha);
        g.drawImageTransformed(image, fit.followedBy(juce::AffineTransform::scale(scale, scale, about.x, about.y)));
        g.setOpacity(1.0f);
    };
    g.fillAll(colours::ink);
    if (transition.kind == 1) {          // into a place: the map zooms towards the landmark, the scene settles in
        drawScaled(transition.from, 1.0f + 2.4f * t, focus, 1.0f);
        drawScaled(transition.to, 1.18f - 0.18f * t, bounds.getCentre(), t);
    } else if (transition.kind == 2) {   // out to the map
        drawScaled(transition.from, 1.0f + 0.18f * t, bounds.getCentre(), 1.0f);
        drawScaled(transition.to, 3.4f - 2.4f * t, focus, t);
    } else {
        drawScaled(transition.from, 1.0f, bounds.getCentre(), 1.0f);
        drawScaled(transition.to, 1.0f, bounds.getCentre(), t);
    }
}

void DungeonCrawlerEditor::paintMapOverlay(juce::Graphics& g) {
    const auto& marks = dc::art::mapLandmarks();
    // The current location: a blood-red ring with a soft glow.
    if (shown >= 0) {
        const auto& l = marks[static_cast<std::size_t>(shown)];
        const auto c = toLocal(l.position);
        const float r = l.radius * static_cast<float>(getWidth()) / dc::art::virtualWidth;
        g.setGradientFill(juce::ColourGradient(colours::blood.withAlpha(0.32f), c, colours::blood.withAlpha(0.0f), c.translated(r * 1.5f, 0), true));
        g.fillEllipse(juce::Rectangle<float>(3 * r, 3 * r).withCentre(c));
        g.setColour(colours::blood);
        g.drawEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c), 2.2f);
        g.drawEllipse(juce::Rectangle<float>(2 * r + 7, 2 * r + 7).withCentre(c), 0.8f);
    }
    // The journey: a dashed red path, drawn as far as the sound has travelled.
    if (pathFrom >= 0 && pathTo >= 0 && pathFrom != pathTo) {
        const auto a = toLocal(marks[static_cast<std::size_t>(pathFrom)].position), b = toLocal(marks[static_cast<std::size_t>(pathTo)].position);
        const float progress = processor.travelProgress.load() < 1.0f ? processor.travelProgress.load() : 1.0f;
        const float fade = pathEnded > 0 ? juce::jlimit(0.0f, 1.0f, 1.0f - static_cast<float>((nowSeconds() - pathEnded) / 1.5)) : 1.0f;
        const auto mid = (a + b) * 0.5f + juce::Point<float>(-(b.y - a.y), b.x - a.x) * 0.18f;
        juce::Path curve;
        curve.startNewSubPath(a);
        curve.quadraticTo(mid, b);
        juce::Path partial;
        const float length = curve.getLength();
        juce::PathStrokeType(2.4f).createDashedStroke(partial, curve, std::array<float, 2> { 7.0f, 5.0f }.data(), 2);
        g.saveState();
        // Reveal the dashes up to the travelled point.
        const auto head = curve.getPointAlongPath(length * progress);
        juce::Path reveal;
        reveal.addEllipse(juce::Rectangle<float>(1, 1).withCentre(a));
        for (int k = 0; k <= 24; ++k) {
            const auto pt = curve.getPointAlongPath(length * progress * static_cast<float>(k) / 24.0f);
            reveal.addEllipse(juce::Rectangle<float>(16, 16).withCentre(pt));
        }
        g.reduceClipRegion(reveal);
        g.setColour(colours::blood.withAlpha(0.9f * fade));
        g.fillPath(partial);
        g.restoreState();
        g.setColour(colours::blood.withAlpha(fade));
        g.fillEllipse(juce::Rectangle<float>(7, 7).withCentre(head));
    }
    // The hovered landmark's name, on a dark plate.
    if (hovered >= 0) {
        const auto& l = marks[static_cast<std::size_t>(hovered)];
        const auto c = toLocal(l.position);
        const juce::String text = juce::String(dc::locationNames[static_cast<std::size_t>(hovered)]) + "  \xc2\xb7  "
                                + juce::String(dc::subLocationCount(hovered) + 1) + " places";
        const auto font = titleFont(17.0f);
        const float width = juce::GlyphArrangement::getStringWidth(font, juce::String::fromUTF8(text.toRawUTF8())) + 24.0f;
        auto plate = juce::Rectangle<float>(width, 28.0f).withCentre({ c.x, c.y - l.radius * static_cast<float>(getWidth()) / dc::art::virtualWidth - 22.0f });
        plate = plate.withX(juce::jlimit(6.0f, static_cast<float>(getWidth()) - width - 6.0f, plate.getX()));
        g.setColour(juce::Colours::black.withAlpha(0.78f));
        g.fillRoundedRectangle(plate, 4.0f);
        g.setColour(colours::blood);
        g.drawRoundedRectangle(plate, 4.0f, 1.0f);
        g.setColour(colours::ivory);
        g.setFont(font);
        g.drawText(juce::String::fromUTF8(text.toRawUTF8()), plate, juce::Justification::centred);
    }
}

void DungeonCrawlerEditor::paint(juce::Graphics& g) {
    const double now = nowSeconds();
    const auto px = pixelSize();
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    if (transition.active) paintTransition(g, now);
    else {
        const auto image = cache.get(scene, px.x, px.y);
        if (image.isValid()) g.drawImage(image, getLocalBounds().toFloat());
        else {
            g.fillAll(colours::ink);
            g.setColour(colours::ash.withAlpha(0.6f));
            g.setFont(labelFont(14.0f));
            g.drawText(scene.isMap() ? "engraving the map..." : "engraving...", getLocalBounds(), juce::Justification::centred);
        }
        if (view == View::Map && !pending) paintMapOverlay(g);
    }
    // Breadcrumb plate.
    const auto crumbs = juce::Rectangle<float>(8, 8, static_cast<float>((placeCrumb.isVisible() ? placeCrumb.getX() + juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont(18.0f), placeCrumb.getText())) + 14 : locationCrumb.getRight()) + 6 - 8), 40);
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.fillRoundedRectangle(crumbs, 5.0f);
    // The place list plate.
    if (view == View::Place && !transition.active && shown >= 0) {
        const int count = dc::subLocationCount(shown);
        g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(getWidth()) - 194.0f, 8.0f, 186.0f, static_cast<float>((count + 1) * 26 + 16)), 5.0f);
    }
    // The controls strip: translucent ink over the art.
    if (controlsShown) {
        const auto strip = controlsArea();
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f), strip.getX(), strip.getY(),
                                               juce::Colours::black.withAlpha(0.86f), strip.getX(), strip.getY() + 40.0f, false));
        g.fillRect(strip);
        g.setColour(colours::dust.withAlpha(0.7f));
        g.drawHorizontalLine(juce::roundToInt(strip.getY()), strip.getX(), strip.getRight());
        g.setColour(colours::blood.withAlpha(0.6f));
        g.drawHorizontalLine(juce::roundToInt(strip.getY()) + 2, strip.getX() + 20.0f, strip.getX() + 120.0f);
    }
}

void DungeonCrawlerEditor::resized() {
    const int w = getWidth(), h = getHeight();
    const int top = h - static_cast<int>(stripHeight);
    const auto place = [this](Fader& f, juce::Rectangle<int> r) {
        f.label.setBounds(r.removeFromBottom(18));
        f.slider.setBounds(r.reduced(2, 0));
        f.label.setVisible(controlsShown && f.label.isVisible() == f.label.isVisible());
    };
    const int faderHeight = static_cast<int>(stripHeight) - 14;
    macroHeading.setBounds(18, top + 2, 120, 14);
    for (int m = 0; m < dc::macroCount; ++m)
        place(macros[static_cast<std::size_t>(m)], { 14 + m * 54, top + 14, 54, faderHeight - 10 });
    const int mainX = 250;
    place(input, { mainX, top + 6, 62, faderHeight });
    place(intensity, { mainX + 62, top + 6, 62, faderHeight });
    place(mix, { mainX + 124, top + 6, 62, faderHeight });
    place(output, { mainX + 186, top + 6, 62, faderHeight });
    travelLabel.setBounds(mainX + 266, top + 14, 56, 24);
    travelTime.setBounds(mainX + 320, top + 14, 230, 24);
    travelSync.setBounds(mainX + 320, top + 46, 120, 24);
    bypass.setBounds(w - 214, top + 70, 96, 30);
    advancedToggle.setBounds(w - 114, top + 70, 100, 30);
    controlsToggle.setBounds(w - 40, controlsShown ? top - 30 : h - 32, 30, 26);
    for (auto* c : std::initializer_list<juce::Component*> { &input.slider, &input.label, &intensity.slider, &intensity.label, &mix.slider, &mix.label,
                                                             &output.slider, &output.label, &travelLabel, &travelTime, &travelSync, &bypass, &advancedToggle, &macroHeading })
        c->setVisible(controlsShown);
    if (shown >= 0)
        for (int m = 0; m < dc::macroCount; ++m) {
            const bool used = m < dc::macroCountFor(shown);
            macros[static_cast<std::size_t>(m)].slider.setVisible(used && controlsShown);
            macros[static_cast<std::size_t>(m)].label.setVisible(used && controlsShown);
        }
    drawer.setBounds(200, 56, w - 214, (controlsShown ? top : h) - 64);
    layoutOverlays();
    const auto px = pixelSize();
    cache.request(scene, px.x, px.y);
    wakeTimer();
}
