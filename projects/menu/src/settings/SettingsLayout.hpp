#pragma once
#include <nxui/core/Types.hpp>
#include <vector>

namespace settings {

enum class ControlKind { None, Toggle, Slider, Selector, Action };

namespace metrics {

// Tab rail, vertical form (Theme Shop, Game Options, Folder Options)
inline constexpr float kTabRailInset  = 14.f;
inline constexpr float kTabCardGap    = 10.f;
inline constexpr float kTabRowHeight  = 58.f;  // card height + gap

// Horizontal tab rail
inline constexpr float kRailHeight     = 96.f;
inline constexpr float kRailTopPad     = 12.f;
inline constexpr float kRailContentGap = 10.f;

inline constexpr float kTileW      = 112.f;  // column width, gap excluded
inline constexpr float kTileGap    = 10.f;
inline constexpr float kTileH      = 50.f;
inline constexpr float kTileHSel   = 60.f;
inline constexpr float kTileLift   = 6.f;
inline constexpr float kTileRadius = 14.f;
inline constexpr float kTileCardH  = kTileHSel + kTileLift;

nxui::Rect tileChipRect(const nxui::Rect& card, bool selected);  // holds the label

// Content list
inline constexpr float kContentCardInsetX = 18.f;
inline constexpr float kContentCardInsetY = 8.f;
inline constexpr float kContentTopPad     = 16.f;
inline constexpr float kContentBottomPad  = 20.f;
inline constexpr float kSectionInsetY     = 1.f;
inline constexpr float kRowShrinkY        = 6.f;  // card is shorter than its slot
inline constexpr float kSectionShrinkY    = 2.f;

// Row internals, mirrored from SettingRowBase::prepareLayout
inline constexpr float kRowHorizontalInset = 10.f;
inline constexpr float kRowColumnGap       = 12.f;

// Controls
inline constexpr float kToggleW      = 64.f;
inline constexpr float kToggleH      = 32.f;
inline constexpr float kSliderTrackH = 14.f;  // as drawn; the hit-test used to assume 12
inline constexpr float kSliderKnobW  = 18.f;
inline constexpr float kSliderPctW   = 44.f;
inline constexpr float kSliderGap    = 10.f;
inline constexpr float kSliderTrackMin = 110.f;
inline constexpr float kSliderTrackMax = 260.f;

inline constexpr float kChevronSize      = 11.f;
inline constexpr float kChevronThickness = 2.f;

// Dropdown
inline constexpr float kDropdownOptH   = 46.f;
inline constexpr float kDropdownPad    = 16.f;
inline constexpr int   kDropdownMaxVis = 6;

inline constexpr float kTouchSlack = 10.f; // tolerance added around a control

// Data-only pages
inline constexpr float kStatColumns  = 2.f;
inline constexpr float kStatGapX     = 12.f;
inline constexpr float kStatRowH     = 82.f;
inline constexpr float kStatGapY     = 10.f;

float      preferredRightWidth(ControlKind kind, float cardWidth);
nxui::Rect rightZone(const nxui::Rect& card, ControlKind kind);
nxui::Rect controlRect(const nxui::Rect& card, ControlKind kind);  // empty when None
nxui::Rect sliderTrackRect(const nxui::Rect& card);                // without touch slack
float      sliderValueAt(const nxui::Rect& track, float touchX);   // in [0,1]

} // namespace metrics

struct RowLayout {
    nxui::Rect slot;     // full vertical slot, screen space
    nxui::Rect card;     // the drawn card
    nxui::Rect control;  // right-hand control; zero-sized when there is none
    float slotOffsetY = 0.f;  // slot top relative to the content top, unscrolled
    float slotHeight  = 0.f;
    
    float textScale  = 1.f;
    float depthAlpha = 1.f;
    int   rawIndex    = -1;
    int   focusIndex  = -1;   // -1 when the row is not focusable
    bool  visible     = false;
    bool  isData      = false;  // read-only readout, drawn as a data block
    ControlKind control_kind = ControlKind::None;
};

struct SettingsLayout {
    nxui::Rect panel{};
    nxui::Rect tabs{};
    nxui::Rect content{};

    std::vector<nxui::Rect> tabCards;  // parallel to the tab list
    std::vector<RowLayout>  rows;      // parallel to the current tab's items

    nxui::Rect dropdown{};  // empty when none is open
    int   dropdownVisibleCount = 0;
    float dropdownVisualStart  = 0.f;

    float totalHeight = 0.f;
    float maxScroll   = 0.f;
    bool  statGrid    = false;  // readout grid, nothing operable

    int tabAt(float x, float y) const;  // tab index under a point, or -1
    int rowAt(float x, float y) const;  // focus index under a point, or -1
    int dropdownOptionAt(float x, float y, int optionCount) const;

    const RowLayout* rowByFocus(int focusIndex) const;
    const RowLayout* rowByRaw(int rawIndex) const;
};

} // namespace settings
