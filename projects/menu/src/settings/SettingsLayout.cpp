#include "SettingsLayout.hpp"
#include <algorithm>
#include <cmath>

namespace settings {

namespace metrics {

float preferredRightWidth(ControlKind kind, float cardWidth) {
    switch (kind) {
        case ControlKind::Toggle:   return std::max(120.f, cardWidth * 0.42f);
        case ControlKind::Slider:   return std::max(170.f, cardWidth * 0.42f);
        case ControlKind::Selector: return std::max(170.f, cardWidth * 0.38f);
        case ControlKind::Action:   return kChevronSize + kRowHorizontalInset * 2.f;
        case ControlKind::None:     return 0.f;
    }
    return 0.f;
}

nxui::Rect tileChipRect(const nxui::Rect& card, bool selected) {
    const float h = selected ? kTileHSel : kTileH;
    return { card.x, card.y + (kTileCardH - h) * 0.5f, card.width, h };
}

nxui::Rect rightZone(const nxui::Rect& card, ControlKind kind) {
    if (kind == ControlKind::None)
        return {};

    const float innerX = card.x + kRowHorizontalInset;
    const float innerW = std::max(0.f, card.width - kRowHorizontalInset * 2.f);
    const float rightW = std::clamp(preferredRightWidth(kind, card.width), 0.f, innerW);
    if (rightW <= 0.f)
        return {};

    const float leftW = std::max(0.f, innerW - rightW - kRowColumnGap);
    return { innerX + leftW + kRowColumnGap, card.y, rightW, card.height };
}

nxui::Rect sliderTrackRect(const nxui::Rect& card) {
    const nxui::Rect right = rightZone(card, ControlKind::Slider);
    if (right.width <= 0.f)
        return {};

    const float trackW = std::clamp(right.width - kSliderPctW - kSliderGap,
                                    kSliderTrackMin, kSliderTrackMax);
    const float pctX   = right.right() - kSliderPctW;
    return { pctX - kSliderGap - trackW,
             right.y + (right.height - kSliderTrackH) * 0.5f,
             trackW, kSliderTrackH };
}

nxui::Rect controlRect(const nxui::Rect& card, ControlKind kind) {
    const nxui::Rect right = rightZone(card, kind);
    if (right.width <= 0.f)
        return {};

    switch (kind) {
        case ControlKind::Toggle:
            return { right.right() - kToggleW,
                     right.y + (right.height - kToggleH) * 0.5f,
                     kToggleW, kToggleH };

        case ControlKind::Slider:
            return sliderTrackRect(card);

        case ControlKind::Selector: {
            const float w = std::max(170.f, right.width);
            const float h = std::max(36.f, card.height - 22.f);
            return { right.right() - w, right.y + (right.height - h) * 0.5f, w, h };
        }

        case ControlKind::Action:
            return card;

        case ControlKind::None:
            break;
    }
    return {};
}

float sliderValueAt(const nxui::Rect& track, float touchX) {
    const float span = std::max(1.f, track.width - kSliderKnobW);
    return std::clamp((touchX - track.x - kSliderKnobW * 0.5f) / span, 0.f, 1.f);
}

} // namespace metrics

int SettingsLayout::tabAt(float x, float y) const {
    for (int i = 0; i < (int)tabCards.size(); ++i) {
        if (tabCards[i].contains(x, y))
            return i;
    }
    return -1;  
}

int SettingsLayout::rowAt(float x, float y) const {
    for (const RowLayout& row : rows) {
        if (row.focusIndex < 0 || !row.visible)
            continue;
        if (row.card.contains(x, y))
            return row.focusIndex;
    }
    return -1;
}

int SettingsLayout::dropdownOptionAt(float x, float y, int optionCount) const {
    if (dropdown.width <= 0.f || dropdownVisibleCount <= 0 || optionCount <= 0)
        return -1;
    if (!dropdown.expanded(metrics::kTouchSlack * 1.8f).contains(x, y))
        return -1;

    const int start = std::clamp((int)std::floor(dropdownVisualStart),
                                 0, std::max(0, optionCount - dropdownVisibleCount));
    const float rowOffset = (float)start - dropdownVisualStart;

    const float span  = metrics::kDropdownOptH * (float)dropdownVisibleCount;
    const float local = std::clamp(y - dropdown.y - 9.f, 0.f, span - 0.001f);
    const int   idx   = start + (int)std::floor(local / metrics::kDropdownOptH - rowOffset);
    return std::clamp(idx, 0, optionCount - 1);
}

const RowLayout* SettingsLayout::rowByFocus(int focusIndex) const {
    if (focusIndex < 0)
        return nullptr;
    for (const RowLayout& row : rows) {
        if (row.focusIndex == focusIndex)
            return &row;
    }
    return nullptr;
}

const RowLayout* SettingsLayout::rowByRaw(int rawIndex) const {
    if (rawIndex < 0 || rawIndex >= (int)rows.size())
        return nullptr;
    return &rows[(size_t)rawIndex];  // rows are built one per item, in order
}

} // namespace settings
