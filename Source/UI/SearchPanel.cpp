#include "SearchPanel.h"
#include "EngravedLookAndFeel.h"

namespace dc::ui {
namespace {
const juce::Colour stoneTop { 0xff1d1a20 }, stoneBottom { 0xff0d0b0f }, iron { 0xff3e3a44 }, inset { 0xff09080b };
// "·" in UTF-8.
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");

void drawMagnifier(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness) {
    const float s = juce::jmin(r.getWidth(), r.getHeight());
    const auto lens = juce::Rectangle<float>(s * 0.62f, s * 0.62f).withPosition(r.getX() + (r.getWidth() - s) * 0.5f + s * 0.06f, r.getY() + (r.getHeight() - s) * 0.5f + s * 0.06f);
    g.setColour(c);
    g.drawEllipse(lens, thickness);
    const auto from = lens.getCentre() + juce::Point<float>(lens.getWidth() * 0.36f, lens.getHeight() * 0.36f);
    g.drawLine({ from, from + juce::Point<float>(s * 0.27f, s * 0.27f) }, thickness * 1.5f);
}

// The small icon of a result: a note (instrument), a pin (place), a sparkle (preset), a
// dotted path (journey), the magnifier (a Recent search).
void drawIcon(juce::Graphics& g, juce::Rectangle<float> r, const dc::search::Entry* e, juce::Colour c) {
    const auto centre = r.getCentre();
    g.setColour(c);
    if (e == nullptr) { drawMagnifier(g, r.reduced(2.0f), c, 1.3f); return; }
    if (e->kind == dc::search::Kind::Instrument) {
        // A quaver: a tilted head, a stem and a flag.
        const auto head = juce::Rectangle<float>(8.0f, 6.0f).withCentre(centre.translated(-2.5f, 5.0f));
        juce::Path p;
        p.addEllipse(head);
        p.applyTransform(juce::AffineTransform::rotation(-0.45f, head.getCentreX(), head.getCentreY()));
        g.fillPath(p);
        const float sx = head.getRight() - 0.6f;
        g.drawLine(sx, head.getCentreY() - 1.0f, sx, centre.y - 8.0f, 1.4f);
        juce::Path flag;
        flag.startNewSubPath(sx, centre.y - 8.0f);
        flag.quadraticTo(sx + 6.0f, centre.y - 4.5f, sx + 3.5f, centre.y + 0.5f);
        g.strokePath(flag, juce::PathStrokeType(1.4f));
    } else if (e->kind == dc::search::Kind::Place) {
        // A map pin; a location (its landmark) gets a little tower on top instead.
        if (e->family == "Location") {
            const auto body = juce::Rectangle<float>(9.0f, 11.0f).withCentre(centre.translated(0.0f, 2.0f));
            g.drawRect(body, 1.3f);
            for (int k = 0; k < 3; ++k) g.fillRect(body.getX() + float(k) * 3.4f, body.getY() - 3.0f, 2.2f, 3.0f);
            g.fillRect(juce::Rectangle<float>(2.6f, 4.0f).withCentre({ centre.x, body.getBottom() - 2.0f }));
        } else {
            juce::Path pin;
            pin.addCentredArc(centre.x, centre.y - 2.5f, 5.0f, 5.0f, 0.0f, -2.4f, 2.4f, true);
            pin.lineTo(centre.x, centre.y + 8.0f);
            pin.closeSubPath();
            g.strokePath(pin, juce::PathStrokeType(1.3f));
            g.fillEllipse(juce::Rectangle<float>(3.4f, 3.4f).withCentre(centre.translated(0.0f, -2.5f)));
        }
    } else if (e->journey) {
        // Three stops joined by a dotted path.
        const juce::Point<float> a = centre.translated(-6.5f, 5.0f), b = centre.translated(0.0f, -1.5f), d = centre.translated(6.5f, -6.0f);
        juce::Path path;
        path.startNewSubPath(a);
        path.lineTo(b);
        path.lineTo(d);
        juce::Path dashed;
        const float dashes[] { 2.0f, 2.0f };
        juce::PathStrokeType(1.1f).createDashedStroke(dashed, path, dashes, 2);
        g.fillPath(dashed);
        for (auto p : { a, b, d }) g.fillEllipse(juce::Rectangle<float>(3.6f, 3.6f).withCentre(p));
    } else {
        // A four-pointed sparkle.
        juce::Path s;
        const float o = 7.5f, i = 1.8f;
        s.startNewSubPath(centre.x, centre.y - o);
        s.lineTo(centre.x + i, centre.y - i);
        s.lineTo(centre.x + o, centre.y);
        s.lineTo(centre.x + i, centre.y + i);
        s.lineTo(centre.x, centre.y + o);
        s.lineTo(centre.x - i, centre.y + i);
        s.lineTo(centre.x - o, centre.y);
        s.lineTo(centre.x - i, centre.y - i);
        s.closeSubPath();
        g.fillPath(s);
    }
}

juce::String tagLine(const dc::search::Entry& e) {
    juce::StringArray t;
    for (std::size_t k = 0; k < e.tags.size() && t.size() < 6; ++k) t.add(juce::String::fromUTF8(e.tags[k].c_str()));
    juce::String line = t.joinIntoString(dot);
    if (!e.recordings.empty()) {
        juce::StringArray r;
        for (const auto& x : e.recordings) r.add(juce::String::fromUTF8(x.c_str()));
        line << "   |   recorded: " << r.joinIntoString(", ");
    }
    return line;
}
} // namespace

//==============================================================================
void SearchButton::paintHover(juce::Graphics& g, bool hover, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(2.0f);
    const bool on = getToggleState();
    // A dark plate so the icon reads on any scene, a crimson ring while the panel is open.
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.62f), r.getCentre(), juce::Colours::black.withAlpha(0.0f),
                                           r.getCentre().translated(r.getWidth() * 0.75f, 0.0f), true));
    g.fillEllipse(r.expanded(2.0f));
    if (hover || down || on) {
        g.setColour((on ? colours::blood : colours::ivory).withAlpha(down ? 0.3f : 0.16f));
        g.fillEllipse(r.reduced(1.0f));
    }
    if (on) {
        g.setColour(colours::blood);
        g.drawEllipse(r.reduced(1.0f), 1.2f);
    }
    drawMagnifier(g, r.reduced(6.0f), on ? colours::ivory : (hover ? colours::ivory : colours::ivory.withAlpha(0.82f)), 1.6f);
}

//==============================================================================
SearchPanel::SearchPanel(const dc::search::Index& idx) : index(idx) {
    setOpaque(true);
    setWantsKeyboardFocus(false);
    field.setFont(labelFont(18.0f));
    field.setIndents(30, 7);
    field.setTextToShowWhenEmpty(index.instrumentPlugin() ? "Search places, instruments, presets, sounds..." : "Search places, journeys, presets, sounds...",
                                 colours::dust.brighter(0.25f));
    field.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    field.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    field.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    field.setColour(juce::TextEditor::textColourId, colours::ivory);
    field.setColour(juce::TextEditor::highlightColourId, colours::blood.withAlpha(0.45f));
    field.setColour(juce::TextEditor::highlightedTextColourId, colours::ivory);
    field.setColour(juce::CaretComponent::caretColourId, colours::ivory);
    field.setSelectAllWhenFocused(true);
    field.setEscapeAndReturnKeysConsumed(true);
    field.onTextChange = [this] { refresh(); };
    field.setTitle("Search");
    addAndMakeVisible(field);
    addAndMakeVisible(list);
    preview.setTooltip("Hold to hear this instrument (a short phrase)");
    addChildComponent(preview);
    std::vector<juce::String> names;
    if (index.instrumentPlugin()) names.push_back("Instruments");
    names.push_back("Places");
    names.push_back("Presets");
    for (const auto& m : dc::search::moodChips()) names.push_back(juce::String(m).substring(0, 1).toUpperCase() + juce::String(m).substring(1));
    for (auto& n : names) chips.push_back({ n, {}, false });
    setSize(panelWidth, maxHeight);
}

SearchPanel::~SearchPanel() { pressPreview(false); }

void SearchPanel::open() {
    for (auto& c : chips) c.on = false;
    closing = false;
    field.setText({}, false);
    setVisible(true);
    toFront(false);
    refresh();
    if (isShowing()) field.grabKeyboardFocus();
}

void SearchPanel::close() {
    if (!isVisible()) return;
    pressPreview(false);
    const auto q = field.getText().trim();
    if (!closing && q.isNotEmpty()) pushRecent("q:" + q);
    closing = false;
    setVisible(false);
    if (onClose) onClose();
}

void SearchPanel::visibilityChanged() {
    if (!isVisible()) pressPreview(false);
}

void SearchPanel::setQuery(const juce::String& text) {
    field.setText(text, false);
    field.moveCaretToEnd();
    refresh();
}

void SearchPanel::toggleChip(int k) {
    if (k < 0 || k >= int(chips.size())) return;
    chips[std::size_t(k)].on = !chips[std::size_t(k)].on;
    refresh();
    repaint(chipArea);
}

void SearchPanel::refresh() {
    const auto start = juce::Time::getMillisecondCounterHiRes();
    const auto q = field.getText().trim();
    dc::search::Filter filter;
    bool anyChip = false;
    for (const auto& c : chips) {
        if (!c.on) continue;
        anyChip = true;
        if (c.name == "Instruments") filter.instruments = true;
        else if (c.name == "Places") filter.places = true;
        else if (c.name == "Presets") filter.presets = true;
        else filter.moods.push_back(c.name.toLowerCase().toStdString());
    }
    rows.clear();
    recentMode = q.isEmpty() && !anyChip;
    if (recentMode) {
        const auto recent = getRecent ? getRecent() : juce::StringArray();
        std::vector<Row> items;
        for (const auto& item : recent) {
            if (item.startsWith("q:")) items.push_back({ false, item.substring(2), -1, true });
            else if (item.startsWith("e:")) {
                const int e = index.indexOf(item.substring(2).toStdString());
                if (e >= 0) items.push_back({ false, {}, e, false });
            }
        }
        if (!items.empty()) {
            rows.push_back({ true, "Recent", -1, false });
            for (auto& r : items) rows.push_back(r);
        }
    } else {
        const auto results = index.search(q.toStdString(), filter, 60);
        int lastKind = -1;
        for (const auto& r : results) {
            const auto& e = index.entries()[std::size_t(r.entry)];
            if (int(e.kind) != lastKind) {
                lastKind = int(e.kind);
                rows.push_back({ true, dc::search::kindName(e.kind), -1, false });
            }
            rows.push_back({ false, {}, r.entry, false });
        }
    }
    selected = -1;
    for (int k = 0; k < int(rows.size()); ++k)
        if (!rows[std::size_t(k)].header) { selected = k; break; }
    scroll = 0;
    ++searchCount;
    lastMs = juce::Time::getMillisecondCounterHiRes() - start;
    updateDescription();
    list.repaint();
}

int SearchPanel::rowTop(int row) const {
    int y = 0;
    for (int k = 0; k < row && k < int(rows.size()); ++k) y += rows[std::size_t(k)].header ? headerHeight : rowHeight;
    return y;
}

int SearchPanel::resultCount() const {
    int n = 0;
    for (const auto& r : rows) n += r.header ? 0 : 1;
    return n;
}

int SearchPanel::rowOfSelectable(int k) const {
    int n = 0;
    for (int r = 0; r < int(rows.size()); ++r)
        if (!rows[std::size_t(r)].header) {
            if (n == k) return r;
            ++n;
        }
    return -1;
}

int SearchPanel::selectedSelectable() const {
    if (selected < 0) return -1;
    int n = 0;
    for (int r = 0; r < selected && r < int(rows.size()); ++r) n += rows[std::size_t(r)].header ? 0 : 1;
    return n;
}

const dc::search::Entry* SearchPanel::resultAt(int k) const {
    const int r = rowOfSelectable(k);
    if (r < 0 || rows[std::size_t(r)].entry < 0) return nullptr;
    return &index.entries()[std::size_t(rows[std::size_t(r)].entry)];
}

juce::String SearchPanel::rowText(int k) const {
    const int r = rowOfSelectable(k);
    if (r < 0) return {};
    const auto& row = rows[std::size_t(r)];
    return row.entry >= 0 ? juce::String::fromUTF8(index.entries()[std::size_t(row.entry)].name.c_str()) : row.text;
}

juce::String SearchPanel::groupHeadings() const {
    juce::StringArray h;
    for (const auto& r : rows) if (r.header) h.add(r.text);
    return h.joinIntoString("|");
}

juce::String SearchPanel::descriptionText() const { return description; }

void SearchPanel::selectResult(int k) {
    const int r = rowOfSelectable(k);
    if (r < 0 || r == selected) return;
    pressPreview(false);
    selected = r;
    ensureVisible();
    updateDescription();
    list.repaint();
}

void SearchPanel::hoverResult(int k) { selectResult(k); }

void SearchPanel::moveSelection(int delta) {
    const int count = resultCount();
    if (count == 0) return;
    const int now = selectedSelectable();
    const int next = now < 0 ? 0 : juce::jlimit(0, count - 1, now + delta);
    selectResult(next);
}

void SearchPanel::ensureVisible() {
    if (selected < 0) return;
    const int top = rowTop(selected), bottom = top + rowHeight, h = listArea.getHeight();
    // Show its group's heading too when it is the group's first.
    const int headTop = selected > 0 && rows[std::size_t(selected - 1)].header ? top - headerHeight : top;
    if (headTop < scroll) scroll = headTop;
    else if (bottom > scroll + h) scroll = bottom - h;
    scroll = juce::jmax(0, scroll);
}

bool SearchPanel::chooseSelected() {
    if (selected < 0) return false;
    choose(selected);
    return true;
}

void SearchPanel::pushRecent(const juce::String& item) {
    if (!getRecent || !setRecent) return;
    auto recent = getRecent();
    recent.removeString(item);
    recent.insert(0, item);
    while (recent.size() > 8) recent.remove(recent.size() - 1);
    setRecent(recent);
}

void SearchPanel::choose(int row) {
    if (row < 0 || row >= int(rows.size()) || rows[std::size_t(row)].header) return;
    const auto r = rows[std::size_t(row)];
    if (r.recentQuery) { setQuery(r.text); return; }
    const auto entry = index.entries()[std::size_t(r.entry)];
    pushRecent("e:" + juce::String(entry.key));
    closing = true;   // a chosen result, not a search left unfinished
    close();
    if (onChoose) onChoose(entry);
}

bool SearchPanel::handleKey(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::downKey) { moveSelection(1); return true; }
    if (key == juce::KeyPress::upKey) { moveSelection(-1); return true; }
    if (key == juce::KeyPress::pageDownKey) { moveSelection(5); return true; }
    if (key == juce::KeyPress::pageUpKey) { moveSelection(-5); return true; }
    if (key == juce::KeyPress::returnKey) { chooseSelected(); return true; }
    return false;
}

void SearchPanel::updateDescription() {
    description = descriptionTitle = {};
    int instrument = -1;
    if (selected >= 0) {
        const auto& r = rows[std::size_t(selected)];
        if (r.recentQuery) {
            descriptionTitle = "Search again";
            description = juce::String::fromUTF8("\xe2\x80\x9c") + r.text + juce::String::fromUTF8("\xe2\x80\x9d");
        } else if (r.entry >= 0) {
            const auto& e = index.entries()[std::size_t(r.entry)];
            descriptionTitle = juce::String::fromUTF8(e.name.c_str());
            description = juce::String::fromUTF8(e.description.c_str());
            if (e.kind == dc::search::Kind::Instrument) instrument = e.instrument;
        }
    }
    preview.setVisible(instrument >= 0 && onPreview != nullptr);
    preview.setEnabled(previewAllowed == nullptr || previewAllowed());
    repaint(descriptionArea);
}

void SearchPanel::pressPreview(bool down) {
    if (down) {
        const int row = selected;
        if (row < 0 || rows[std::size_t(row)].entry < 0) return;
        const auto& e = index.entries()[std::size_t(rows[std::size_t(row)].entry)];
        if (e.kind != dc::search::Kind::Instrument || onPreview == nullptr) return;
        if (previewAllowed != nullptr && !previewAllowed()) { preview.setEnabled(false); return; }
        if (previewing >= 0) onPreview(previewing, false);
        previewing = e.instrument;
        onPreview(previewing, true);
    } else if (previewing >= 0) {
        const int was = previewing;
        previewing = -1;
        if (onPreview) onPreview(was, false);
    }
}

void SearchPanel::PreviewButton::buttonStateChanged() {
    const bool down = getState() == juce::Button::buttonDown;
    if (down != held) {
        held = down;
        owner.pressPreview(down);
    }
}

void SearchPanel::PreviewButton::paintHover(juce::Graphics& g, bool hover, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(down ? colours::blood.withAlpha(0.55f) : inset);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(isEnabled() ? (hover || down ? colours::blood.brighter(0.3f) : iron.brighter(0.5f)) : iron);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
    const auto c = isEnabled() ? colours::ivory : colours::dust;
    juce::Path tri;
    const float x = r.getX() + 11.0f, cy = r.getCentreY();
    tri.addTriangle(x, cy - 5.0f, x, cy + 5.0f, x + 8.0f, cy);
    g.setColour(c);
    g.fillPath(tri);
    g.setFont(labelFont(14.0f));
    g.drawText(isEnabled() ? "Hold to hear" : "Host playing", r.withTrimmedLeft(24.0f), juce::Justification::centred);
}

//==============================================================================
void SearchPanel::resized() {
    auto r = getLocalBounds().reduced(12, 10);
    fieldArea = r.removeFromTop(36);
    field.setBounds(fieldArea.reduced(2, 1));
    r.removeFromTop(8);
    chipArea = r.removeFromTop(24);
    int x = chipArea.getX();
    for (auto& c : chips) {
        const int w = 22 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(14.5f), c.name));
        c.bounds = { x, chipArea.getY(), w, chipArea.getHeight() };
        x += w + 6;
    }
    r.removeFromTop(8);
    descriptionArea = r.removeFromBottom(62);
    r.removeFromBottom(4);
    listArea = r;
    list.setBounds(listArea);
    preview.setBounds(descriptionArea.getRight() - 118, descriptionArea.getY() + 16, 118, 30);
}

void SearchPanel::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    // Dark stone with an iron frame, a crimson edge along the top, rivets at the corners.
    g.setGradientFill(juce::ColourGradient(stoneTop, 0, 0, stoneBottom, 0, b.getHeight(), false));
    g.fillRect(b);
    g.setColour(juce::Colours::white.withAlpha(0.025f));
    for (float y = 9.0f; y < b.getHeight(); y += 23.0f) g.fillRect(0.0f, y, b.getWidth(), 1.0f);   // faint courses of stone
    g.setColour(iron);
    g.drawRect(b, 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRect(b.reduced(2.0f), 1.0f);
    g.setColour(colours::blood);
    g.fillRect(b.getX() + 2.0f, b.getY() + 2.0f, b.getWidth() - 4.0f, 2.0f);
    g.setColour(iron.brighter(0.4f));
    for (auto p : { b.getTopLeft().translated(7, 9), b.getTopRight().translated(-7, 9), b.getBottomLeft().translated(7, -7), b.getBottomRight().translated(-7, -7) })
        g.fillEllipse(juce::Rectangle<float>(3.5f, 3.5f).withCentre(p));
    // The field: an inset well with the magnifier.
    const auto f = fieldArea.toFloat();
    g.setColour(inset);
    g.fillRoundedRectangle(f, 4.0f);
    g.setColour(field.hasKeyboardFocus(true) ? colours::blood.withAlpha(0.8f) : iron.brighter(0.2f));
    g.drawRoundedRectangle(f.reduced(0.5f), 4.0f, 1.0f);
    drawMagnifier(g, juce::Rectangle<float>(f.getX() + 8.0f, f.getCentreY() - 8.0f, 16.0f, 16.0f), colours::ash, 1.4f);
    // The chips.
    g.setFont(labelFont(14.5f));
    for (std::size_t k = 0; k < chips.size(); ++k) {
        const auto& c = chips[k];
        const auto cr = c.bounds.toFloat().reduced(0.5f);
        const bool mood = !(c.name == "Instruments" || c.name == "Places" || c.name == "Presets");
        g.setColour(c.on ? colours::blood.withAlpha(0.85f) : inset.withAlpha(0.9f));
        g.fillRoundedRectangle(cr, cr.getHeight() * 0.5f);
        g.setColour(c.on ? colours::blood.brighter(0.4f) : (mood ? iron.brighter(0.1f) : iron.brighter(0.35f)));
        g.drawRoundedRectangle(cr, cr.getHeight() * 0.5f, 1.0f);
        g.setColour(c.on ? colours::ivory : (mood ? colours::ash.darker(0.15f) : colours::ash));
        g.drawText(c.name, c.bounds, juce::Justification::centred);
    }
    // The description of the selected result.
    const auto d = descriptionArea.toFloat();
    g.setColour(iron);
    g.fillRect(d.getX(), d.getY(), d.getWidth(), 1.0f);
    const float textRight = preview.isVisible() ? float(preview.getX()) - 10.0f : d.getRight();
    if (descriptionTitle.isNotEmpty()) {
        g.setFont(titleFont(13.0f));
        g.setColour(colours::ivory);
        g.drawText(descriptionTitle, juce::Rectangle<float>(d.getX(), d.getY() + 6.0f, textRight - d.getX(), 17.0f), juce::Justification::centredLeft, true);
        g.setFont(labelFont(14.5f));
        g.setColour(colours::ash);
        g.drawFittedText(description, juce::Rectangle<float>(d.getX(), d.getY() + 24.0f, textRight - d.getX(), d.getHeight() - 24.0f).toNearestInt(),
                         juce::Justification::topLeft, 2, 0.9f);
    } else if (rows.empty()) {
        g.setFont(labelFont(14.5f));
        g.setColour(colours::dust.brighter(0.3f));
        g.drawText(recentMode ? "Type a name, a sound or a mood: choir, bells, eerie, guitar..." : "Arrow keys and Enter, or click, to go there.",
                   d.withTrimmedTop(8.0f), juce::Justification::centredLeft, true);
    }
}

void SearchPanel::mouseDown(const juce::MouseEvent& e) {
    for (std::size_t k = 0; k < chips.size(); ++k)
        if (chips[k].bounds.contains(e.getPosition())) { toggleChip(int(k)); return; }
}

//==============================================================================
int SearchPanel::List::rowAt(int y) const {
    int top = -owner.scroll;
    for (int k = 0; k < int(owner.rows.size()); ++k) {
        const int h = owner.rows[std::size_t(k)].header ? headerHeight : rowHeight;
        if (y >= top && y < top + h) return k;
        top += h;
    }
    return -1;
}

void SearchPanel::List::mouseMove(const juce::MouseEvent& e) {
    // Hovering a result selects it (its description shows below).
    const int r = rowAt(e.y);
    if (r >= 0 && !owner.rows[std::size_t(r)].header && r != owner.selected) {
        owner.pressPreview(false);
        owner.selected = r;
        owner.updateDescription();
        repaint();
    }
}

void SearchPanel::List::mouseExit(const juce::MouseEvent&) {}

void SearchPanel::List::mouseDown(const juce::MouseEvent& e) {
    const int r = rowAt(e.y);
    if (r >= 0 && !owner.rows[std::size_t(r)].header) owner.choose(r);
}

void SearchPanel::List::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    const int total = owner.rowTop(int(owner.rows.size()));
    const int next = juce::jlimit(0, juce::jmax(0, total - getHeight()), owner.scroll - juce::roundToInt(w.deltaY * 160.0f));
    if (next != owner.scroll) { owner.scroll = next; repaint(); }
}

void SearchPanel::List::paint(juce::Graphics& g) {
    ++repaints;
    const auto area = getLocalBounds();
    g.setGradientFill(juce::ColourGradient(stoneBottom.brighter(0.08f), 0, 0, stoneBottom, 0, float(area.getHeight()), false));
    g.fillRect(area);
    if (owner.rows.empty()) {
        g.setFont(labelFont(16.0f));
        g.setColour(colours::dust.brighter(0.3f));
        g.drawText(owner.recentMode ? "No recent searches yet" : "Nothing found", area.withTrimmedTop(18).removeFromTop(24), juce::Justification::centred);
        return;
    }
    int top = -owner.scroll;
    const auto& entries = owner.index.entries();
    for (int k = 0; k < int(owner.rows.size()); ++k) {
        const auto& row = owner.rows[std::size_t(k)];
        const int h = row.header ? headerHeight : rowHeight;
        if (top + h < 0) { top += h; continue; }
        if (top > area.getHeight()) break;
        const auto r = juce::Rectangle<int>(0, top, area.getWidth(), h);
        if (row.header) {
            g.setFont(titleFont(11.5f));
            g.setColour(colours::blood.brighter(0.35f));
            g.drawText(row.text.toUpperCase(), r.withTrimmedLeft(6), juce::Justification::centredLeft);
            const float textW = juce::GlyphArrangement::getStringWidth(titleFont(11.5f), row.text.toUpperCase());
            g.setColour(iron);
            g.fillRect(float(r.getX()) + 14.0f + textW, float(r.getCentreY()), float(r.getWidth()) - 20.0f - textW, 1.0f);
        } else {
            const bool sel = k == owner.selected;
            const auto* e = row.entry >= 0 ? &entries[std::size_t(row.entry)] : nullptr;
            if (sel) {
                g.setColour(colours::ivory.withAlpha(0.065f));
                g.fillRect(r.reduced(0, 1));
                g.setColour(colours::blood);
                g.fillRect(r.getX(), r.getY() + 3, 3, r.getHeight() - 6);
            }
            drawIcon(g, juce::Rectangle<float>(10.0f, float(r.getY()) + 6.0f, 20.0f, 20.0f), e, sel ? colours::blood.brighter(0.4f) : colours::ash.darker(0.1f));
            if (e == nullptr) {
                g.setFont(labelFont(16.0f).italicised());
                g.setColour(colours::ash);
                g.drawText(juce::String::fromUTF8("\xe2\x80\x9c") + row.text + juce::String::fromUTF8("\xe2\x80\x9d"), r.withTrimmedLeft(38).withTrimmedRight(8),
                           juce::Justification::centredLeft, true);
            } else {
                const auto where = juce::String::fromUTF8(e->where.c_str());
                g.setFont(labelFont(14.0f));
                const int whereW = juce::jmin(200, 6 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(14.0f), where)));
                g.setColour(colours::ash.withAlpha(sel ? 1.0f : 0.8f));
                g.drawText(where, juce::Rectangle<int>(r.getRight() - whereW - 8, r.getY() + 3, whereW, 18), juce::Justification::centredRight, true);
                g.setFont(titleFont(14.0f));
                g.setColour(sel ? colours::ivory : colours::ivory.withAlpha(0.88f));
                g.drawText(juce::String::fromUTF8(e->name.c_str()), juce::Rectangle<int>(38, r.getY() + 3, r.getWidth() - 38 - whereW - 14, 18),
                           juce::Justification::centredLeft, true);
                g.setFont(labelFont(13.0f));
                g.setColour(colours::dust.brighter(sel ? 0.75f : 0.5f));
                g.drawText(tagLine(*e), juce::Rectangle<int>(38, r.getY() + 21, r.getWidth() - 46, 16), juce::Justification::centredLeft, true);
            }
        }
        top += h;
    }
}
}
