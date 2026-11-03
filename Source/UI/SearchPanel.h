#pragma once
// Session 10: the search panel. A magnifier at the top of the window (or Cmd+F) opens it
// over the scene: a field, quick filter chips (Instruments, Places, Presets, and the moods
// Dark, Epic, Eerie, Sacred), the results as you type, grouped by kind with a small icon,
// where each belongs and its tags, and under them the selected result's description (with
// a hold-to-hear button for instruments). With nothing typed it shows the last eight
// searches and results chosen ("Recent").
//
// Arrow keys move through the results, Enter (or a click) chooses: the view then does
// exactly what choosing it by hand does and closes the panel. Esc or a click outside closes
// it; it never takes the clicks meant for the map.
//
// Drawing: the panel is opaque (the scene behind is never repainted for it) and repaints
// only itself, on typing, hovering or moving the selection. The index is built once at
// load; a search is a scan of about 130 entries (well under a millisecond).
#include "Hover.h"
#include "../World/Search.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dc::ui {

// The magnifier at the top of the window.
class SearchButton final : public HoverButton {
public:
    SearchButton() : HoverButton("Search") {}
    void paintHover(juce::Graphics&, bool hover, bool down) override;
};

class SearchPanel final : public juce::Component {
public:
    static constexpr int panelWidth = 470, maxHeight = 452;
    explicit SearchPanel(const dc::search::Index&);
    ~SearchPanel() override;

    // The view's side: what a chosen result does, closing, Recent, the preview.
    std::function<void(const dc::search::Entry&)> onChoose;
    std::function<void()> onClose;
    std::function<juce::StringArray()> getRecent;
    std::function<void(const juce::StringArray&)> setRecent;
    std::function<void(int instrument, bool down)> onPreview;
    std::function<bool()> previewAllowed;   // false while the host plays

    // Shows the panel with an empty field (Recent), the field focused.
    void open();
    // Hides it (a query typed and not chosen goes into Recent).
    void close();
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // What a key does (the field forwards arrows, Enter, Esc, Page up/down to here).
    bool handleKey(const juce::KeyPress&);

    // Tests.
    void setQuery(const juce::String& text);   // as typed
    juce::String query() const { return field.getText(); }
    juce::TextEditor& fieldForTest() { return field; }
    int resultCount() const;                    // selectable rows (results or Recent)
    const dc::search::Entry* resultAt(int k) const;   // nullptr for a Recent search
    juce::String rowText(int k) const;          // the name shown (a Recent search's text)
    int selectedResult() const { return selectedSelectable(); }
    void selectResult(int k);
    void hoverResult(int k);                    // as the mouse over row k
    bool chooseSelected();                      // as Enter
    juce::String descriptionText() const;
    bool showingRecent() const noexcept { return recentMode; }
    juce::String groupHeadings() const;         // "Places|Instruments|Presets" in order shown
    int chipCount() const { return int(chips.size()); }
    juce::String chipName(int k) const { return chips[std::size_t(k)].name; }
    bool chipOn(int k) const { return chips[std::size_t(k)].on; }
    void toggleChip(int k);
    juce::Component& listForTest() { return list; }
    juce::Button& previewButtonForTest() { return preview; }
    // The preview button pressed or let go (its mouse does this too).
    void pressPreview(bool down);
    int previewingInstrument() const noexcept { return previewing; }
    bool previewVisible() const { return preview.isVisible(); }
    int listRepaints() const noexcept { return list.repaints; }
    int searches() const noexcept { return searchCount; }
    double lastSearchMs() const noexcept { return lastMs; }

private:
    struct Row {
        bool header = false;
        juce::String text;      // a heading, or a Recent search
        int entry = -1;         // an index entry
        bool recentQuery = false;
    };
    class Field final : public juce::TextEditor {
    public:
        explicit Field(SearchPanel& p) : owner(p) {}
        bool keyPressed(const juce::KeyPress& k) override { return owner.handleKey(k) || juce::TextEditor::keyPressed(k); }
    private:
        SearchPanel& owner;
    };
    class List final : public juce::Component {
    public:
        explicit List(SearchPanel& p) : owner(p) { setOpaque(true); }
        void paint(juce::Graphics&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        int rowAt(int y) const;
        int repaints = 0;
    private:
        SearchPanel& owner;
    };
    // Hold to hear: a short phrase on the selected instrument.
    class PreviewButton final : public HoverButton {
    public:
        explicit PreviewButton(SearchPanel& p) : HoverButton("Preview"), owner(p) {}
        void paintHover(juce::Graphics&, bool hover, bool down) override;
        void buttonStateChanged() override;
    private:
        SearchPanel& owner;
        bool held = false;
    };
    struct Chip { juce::String name; juce::Rectangle<int> bounds; bool on = false; };

    void refresh();                 // runs the search, rebuilds the rows
    void moveSelection(int delta);
    void ensureVisible();
    void choose(int row);
    void pushRecent(const juce::String& item);
    int selectedSelectable() const;
    int rowOfSelectable(int k) const;
    void updateDescription();
    void mouseDown(const juce::MouseEvent&) override;   // the chips
    static constexpr int rowHeight = 40, headerHeight = 22;
    int rowTop(int row) const;

    const dc::search::Index& index;
    Field field { *this };
    List list { *this };
    PreviewButton preview { *this };
    std::vector<Chip> chips;
    std::vector<Row> rows;
    int selected = -1;              // row index
    int scroll = 0;                 // pixels
    bool recentMode = true;
    juce::String description, descriptionTitle;
    int previewing = -1;
    int searchCount = 0;
    double lastMs = 0;
    bool closing = false;
    juce::Rectangle<int> fieldArea, chipArea, listArea, descriptionArea;
};
}
