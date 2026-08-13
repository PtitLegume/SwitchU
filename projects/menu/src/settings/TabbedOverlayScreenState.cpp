#include "TabbedOverlayScreen.hpp"
#include "core/DebugLog.hpp"

#include <algorithm>

namespace {

float easeOutCubic(float t) {
    float f = 1.f - t;
    return 1.f - f * f * f;
}

float easeInCubic(float t) {
    return t * t * t;
}

} // namespace

void TabbedOverlayScreen::refreshTranslations() {
    DebugLog::log("[settings] refreshTranslations()");
    invalidateLayout();  // new labels mean new wrapped heights
    int oldTab = m_tabIndex;
    int oldContent = m_contentIdx;

    buildTabs();

    if (!m_tabs.empty()) {
        m_tabIndex = std::clamp(oldTab, 0, (int)m_tabs.size() - 1);
    } else {
        m_tabIndex = 0;
    }

    clampContentIdx();
    if (focusableCount() > 0)
        m_contentIdx = std::clamp(oldContent, 0, focusableCount() - 1);
    else
        m_contentIdx = 0;

    rebuildTabBar();
    rebuildContentItems();
}

void TabbedOverlayScreen::rebuildCurrentTab() {
    invalidateLayout();
    int oldTab = m_tabIndex;
    int oldFocus = m_contentIdx;
    float oldScroll = m_scrollTarget;

    const bool needsStructuralRebuild = !usesCustomContentLayout() || m_tabs.empty();
    if (needsStructuralRebuild)
        buildTabs();

    if (!m_tabs.empty()) {
        m_tabIndex = std::clamp(oldTab, 0, (int)m_tabs.size() - 1);
    } else {
        m_tabIndex = 0;
    }

    clampContentIdx();
    if (focusableCount() > 0)
        m_contentIdx = std::clamp(oldFocus, 0, focusableCount() - 1);
    else
        m_contentIdx = 0;
    m_scrollTarget = oldScroll;
    m_scrollY = oldScroll;

    if (needsStructuralRebuild || !usesCustomContentLayout())
        rebuildTabBar();
    rebuildContentItems();
}

void TabbedOverlayScreen::refreshCurrentTabWidgets() {
    if (usesCustomContentLayout()) {
        rebuildContentItems();
        return;
    }

    if (m_tabIndex >= 0 && m_tabIndex < (int)m_cachedTabContentWidgets.size())
        m_cachedTabContentWidgets[(size_t)m_tabIndex].clear();

    clampContentIdx();
    m_contentIdx = std::clamp(m_contentIdx, 0, std::max(0, focusableCount() - 1));
    rebuildContentItems();
}

void TabbedOverlayScreen::requestDialog(const std::string& title, const std::string& msg,
                                   std::vector<DialogButtonDef> buttons) {
    if (m_dialogRequestCb) m_dialogRequestCb(title, msg, std::move(buttons));
}

void TabbedOverlayScreen::requestToast(const std::string& msg, float holdSeconds) {
    if (msg.empty()) return;
    m_toastText = msg;
    m_trackToastHold = std::max(0.f, holdSeconds);
    m_trackToastFading = false;
    m_trackToastAnim.setImmediate(1.f);
}

void TabbedOverlayScreen::warmup() {
    int oldTab = m_tabIndex;
    int oldContent = m_contentIdx;
    float oldScrollTarget = m_scrollTarget;
    float oldScrollY = m_scrollY;

    buildTabs();

    if (m_tabs.empty()) {
        m_tabIndex = 0;
        m_contentIdx = 0;
        m_scrollTarget = 0.f;
        m_scrollY = 0.f;
        return;
    }

    m_tabIndex = std::clamp(oldTab, 0, (int)m_tabs.size() - 1);
    clampContentIdx();
    if (focusableCount() > 0)
        m_contentIdx = std::clamp(oldContent, 0, focusableCount() - 1);
    else
        m_contentIdx = 0;
    m_scrollTarget = oldScrollTarget;
    m_scrollY = oldScrollY;

    rebuildTabBar();

    rebuildContentItems();
}

std::vector<TabbedOverlayScreen::SettingItem>& TabbedOverlayScreen::currentItems() {
    if (m_detailOpen)
        return m_detailItems;
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size())
        return m_noItems;   // per-instance
    return m_tabs[(size_t)m_tabIndex].items;
}

const std::vector<TabbedOverlayScreen::SettingItem>& TabbedOverlayScreen::currentItems() const {
    return const_cast<TabbedOverlayScreen*>(this)->currentItems();
}

void TabbedOverlayScreen::openDetailPage(std::vector<SettingItem> items) {
    m_detailItems = std::move(items);
    m_detailOpen = true;
    m_focusArea = m_detailItems.empty() ? FocusArea::Tabs : FocusArea::Content;
    m_contentIdx = 0;
    m_scrollY = 0.f;
    m_scrollTarget = 0.f;
    closeDropdown(false);
    invalidateLayout();
    rebuildContentItems();
    clampContentIdx();
    m_tabSwitchDir = 1;
    m_contentStaggerT = 0.f;
    m_contentSlideAnim.setImmediate(0.f);
    m_contentSlideAnim.set(1.f, 0.28f, nxui::Easing::outCubic);
    announceCurrentFocus();
}

bool TabbedOverlayScreen::closeDetailPage() {
    if (!m_detailOpen)
        return false;
    m_detailOpen = false;
    m_detailItems.clear();
    m_focusArea = currentItems().empty() ? FocusArea::Tabs : FocusArea::Content;
    m_contentIdx = 0;
    m_scrollY = 0.f;
    m_scrollTarget = 0.f;
    closeDropdown(false);
    invalidateLayout();
    rebuildContentItems();
    clampContentIdx();
    m_tabSwitchDir = -1;
    m_contentStaggerT = 0.f;
    m_contentSlideAnim.setImmediate(0.f);
    m_contentSlideAnim.set(1.f, 0.28f, nxui::Easing::outCubic);
    announceCurrentFocus();
    return true;
}

bool TabbedOverlayScreen::itemFocusable(const SettingItem& item) const {
    (void)item;
    return true;
}

bool TabbedOverlayScreen::listIsDataOnly() const {
    const auto& items = currentItems();
    bool any = false;
    for (const auto& item : items) {
        if (item.type == ItemType::Section)
            continue;
        if (!isDataItem(item.type))
            return false;
        any = true;
    }
    return any;
}

bool TabbedOverlayScreen::tabIsTextOnly() const {
    if (m_tabIndex < 0 || m_tabIndex >= static_cast<int>(m_tabs.size()))
        return false;
    for (const auto& item : currentItems()) {
        if (item.focusable())
            return false;
    }
    return true;
}

int TabbedOverlayScreen::focusableCount() const {
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return 0;
    int count = 0;
    for (auto& item : currentItems())
        if (itemFocusable(item)) ++count;
    return count;
}

int TabbedOverlayScreen::rawIndexFromFocusable(int focIdx) const {
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return 0;
    auto& items = currentItems();
    int count = 0;
    for (int i = 0; i < (int)items.size(); ++i) {
        if (itemFocusable(items[i])) {
            if (count == focIdx) return i;
            ++count;
        }
    }
    return 0;
}

void TabbedOverlayScreen::clampContentIdx() {
    int count = focusableCount();
    if (count <= 0) m_contentIdx = 0;
    else m_contentIdx = std::clamp(m_contentIdx, 0, count - 1);
}

float TabbedOverlayScreen::visibilityProgress() const {
    return std::clamp(panelPopProgress(), 0.f, 1.f);
}

float TabbedOverlayScreen::panelPopProgress() const {
    float t = std::clamp(m_animT, 0.f, 1.f);
    return m_showing ? nxui::Easing::outBack(t) : 1.f - easeInCubic(t);
}

void TabbedOverlayScreen::syncPanelState(float eased) {
    setOpacity(std::clamp(eased, 0.001f, 1.f));
    setScale(0.92f + 0.08f * (m_active || m_animating ? panelPopProgress() : eased));
}

void TabbedOverlayScreen::invalidateBackdropCache() {
    m_backdropCacheValid = false;
    m_cachedPreBlurRadius = -1.f;
    m_cachedBlurIterations = -1;
}

nxui::Rect TabbedOverlayScreen::panelRect() const {
    return rect();
}

nxui::Rect TabbedOverlayScreen::panelRect(float scale) const {
    nxui::Rect panel = panelRect();
    if (scale < 1.f) {
        float width = panel.width * scale;
        float height = panel.height * scale;
        panel.x += (panel.width - width) * 0.5f;
        panel.y += (panel.height - height) * 0.5f;
        panel.width = width;
        panel.height = height;
    }
    return panel;
}

nxui::Rect TabbedOverlayScreen::tabsRect() const {
    nxui::Rect panel = panelRect();
    return tabsRect(panel);
}

nxui::Rect TabbedOverlayScreen::tabsRect(const nxui::Rect& panel) const {
    const float headerH = overlayHeaderHeight();
    if (usesHorizontalTabRail()) {
        return { panel.x + kInnerPad, panel.y + kInnerPad + headerH,
                 std::max(0.f, panel.width - 2 * kInnerPad),
                 settings::metrics::kRailHeight };
    }
    return { panel.x + kInnerPad, panel.y + kInnerPad + headerH,
             overlayTabWidth(), panel.height - 2 * kInnerPad - headerH };
}

nxui::Rect TabbedOverlayScreen::contentRect() const {
    nxui::Rect panel = panelRect();
    return contentRect(panel);
}

nxui::Rect TabbedOverlayScreen::contentRect(const nxui::Rect& panel) const {
    const float headerH = overlayHeaderHeight();
    if (usesHorizontalTabRail()) {
        const float top = tabsRect(panel).bottom() + settings::metrics::kRailContentGap;
        return { panel.x + kInnerPad, top,
                 std::max(0.f, panel.width - 2 * kInnerPad),
                 std::max(0.f, panel.bottom() - kInnerPad - top) };
    }
    float left = panel.x + kInnerPad + overlayTabWidth() + kInnerPad;
    return { left, panel.y + kInnerPad + headerH,
             panel.right() - kInnerPad - left, panel.height - 2 * kInnerPad - headerH };
}

float TabbedOverlayScreen::contentTotalHeight() const {
    return layout().totalHeight;
}

settings::ControlKind TabbedOverlayScreen::controlKindFor(ItemType type) {
    switch (type) {
        case ItemType::Toggle:   return settings::ControlKind::Toggle;
        case ItemType::Slider:   return settings::ControlKind::Slider;
        case ItemType::Selector: return settings::ControlKind::Selector;
        case ItemType::Action:   return settings::ControlKind::Action;
        default:                 return settings::ControlKind::None;
    }
}

nxui::Color TabbedOverlayScreen::currentAccent() const {
    return m_theme ? m_theme->cursorNormal : nxui::Color::white();
}

const settings::SettingsLayout& TabbedOverlayScreen::layout() const {
    using namespace settings;
    using namespace settings::metrics;

    const int itemCount = (m_tabIndex >= 0 && m_tabIndex < (int)m_tabs.size())
        ? (int)currentItems().size() : -1;

    LayoutKey key;
    key.detail     = m_detailOpen;
    key.panel      = panelRect();
    key.tabIndex   = m_tabIndex;
    key.itemCount  = itemCount;
    key.revision   = m_layoutRevision;
    key.scrollY    = m_scrollY;
    key.dropdownRawIdx = (m_dropdownOpen || m_dropdownClosing) ? m_dropdownRawIdx : -1;
    key.dropdownVisualStart = m_dropdownVisualStart;

    if (m_layoutValid && m_layoutKey == key)
        return m_layout;

    SettingsLayout& L = m_layout;
    L = SettingsLayout{};
    L.panel   = key.panel;
    L.tabs    = tabsRect(L.panel);
    L.content = contentRect(L.panel);

    L.tabCards.reserve(m_tabs.size());
    if (usesHorizontalTabRail()) {
        const int n = (int)m_tabs.size();
        if (n > 0) {
            const float pitch = std::min(kTileW + kTileGap, L.tabs.width / (float)n);
            const float tileW = std::max(40.f, pitch - kTileGap);
            const float rowW  = pitch * (float)n - kTileGap;
            const float startX = L.tabs.x + (L.tabs.width - rowW) * 0.5f;
            for (int i = 0; i < n; ++i) {
                const bool selected = (i == m_tabIndex);
                L.tabCards.push_back({ startX + (float)i * pitch,
                                       L.tabs.y + kRailTopPad - (selected ? kTileLift : 0.f),
                                       tileW, kTileCardH });
            }
        }
    } else {
        const float cardW = std::max(0.f, L.tabs.width - kTabRailInset * 2.f);
        const float cardH = kTabRowHeight - kTabCardGap;
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            L.tabCards.push_back({ L.tabs.x + kTabRailInset,
                                   L.tabs.y + kTabRailInset + (float)i * kTabRowHeight,
                                   cardW, cardH });
        }
    }

    if (itemCount < 0) {
        m_layoutKey = key;
        m_layoutValid = true;
        return L;
    }

    const auto& items = currentItems();
    L.rows.reserve(items.size());

    L.statGrid = listIsDataOnly();

    const float innerX = L.content.x + kContentCardInsetX;
    const float innerW = std::max(0.f, L.content.width - kContentCardInsetX * 2.f);
    const float colW   = (innerW - kStatGapX * (kStatColumns - 1.f)) / kStatColumns;

    float offsetY = kContentTopPad;
    int   focusIdx = 0;
    int   column   = 0;   // grid mode only
    for (int i = 0; i < itemCount; ++i) {
        const SettingItem& item = items[(size_t)i];
        const bool isSection = (item.type == ItemType::Section);
        const bool isData = isDataItem(item.type);
        const bool inGrid = L.statGrid && !isSection;
        if (L.statGrid && isSection && column != 0) {
            offsetY += kStatRowH + kStatGapY;
            column = 0;
        }

        const float h = inGrid ? (kStatRowH + kStatGapY)
                               : itemHeight(item, L.content.width);

        RowLayout row;
        row.rawIndex    = i;
        row.slotOffsetY = offsetY;
        row.slotHeight  = h;
        row.slot = { L.content.x, L.content.y + offsetY - m_scrollY, L.content.width, h };

        if (inGrid) {
            row.card = { innerX + (float)column * (colW + kStatGapX),
                         row.slot.y, colW, kStatRowH };
        } else {
            const float insetY = isSection ? kSectionInsetY : isData ? 0.f : kContentCardInsetY;
            const float shrink = isSection ? kSectionShrinkY : isData ? 0.f : kRowShrinkY;
            row.card = { innerX, row.slot.y + insetY, innerW,
                         std::max(0.f, h - shrink) };
        }

        row.control_kind = controlKindFor(item.type);
        row.isData = isData;
        row.focusIndex = itemFocusable(item) ? focusIdx++ : -1;

        L.rows.push_back(row);

        if (inGrid) {
            column = (column + 1) % (int)kStatColumns;
            if (column == 0)
                offsetY += h;
        } else {
            offsetY += h;
        }
    }
    if (L.statGrid && column != 0)
        offsetY += kStatRowH + kStatGapY;   // the trailing odd card still takes a line

    L.totalHeight = offsetY - kContentTopPad;
    L.maxScroll = std::max(0.f, L.totalHeight + kContentTopPad
                                - L.content.height + kContentBottomPad);

    for (RowLayout& row : L.rows) {
        row.control = metrics::controlRect(row.card, row.control_kind);
        row.visible = row.card.bottom() >= L.content.y - 8.f
                   && row.card.y <= L.content.bottom() + 8.f;
    }

    if (key.dropdownRawIdx >= 0 && key.dropdownRawIdx < itemCount) {
        const SettingItem& item = items[(size_t)key.dropdownRawIdx];
        const int total = (int)item.options.size();
        if (total > 0) {
            const int visible = std::min(total, kDropdownMaxVis);
            const float listH = (float)visible * kDropdownOptH + kDropdownPad;
            const RowLayout& row = L.rows[(size_t)key.dropdownRawIdx];

            const nxui::Rect pill = metrics::controlRect(row.card, ControlKind::Selector);
            float dy = row.slot.bottom() + 6.f;
            if (dy + listH > L.content.bottom() - 4.f)
                dy = row.slot.y - listH - 6.f;

            L.dropdown = { pill.x, dy, pill.width, listH };
            L.dropdownVisibleCount = visible;
            L.dropdownVisualStart = (total > visible)
                ? std::clamp(m_dropdownVisualStart, 0.f, (float)(total - visible))
                : 0.f;
        }
    }

    m_layoutKey = key;
    m_layoutValid = true;
    return L;
}
