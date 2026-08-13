#include "TabbedOverlayScreen.hpp"
#include "SettingsGlassTuning.hpp"
#include "SettingItemWidgets.hpp"
#include "core/DebugLog.hpp"
#include <nxui/core/I18n.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/widgets/GlassBox.hpp>
#include <nxui/widgets/Label.hpp>
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
static constexpr float kSettingsBlurRadius = 6.0f;
static constexpr int kSettingsBlurIter = 1;

namespace {

constexpr size_t kWrapHeightCacheLimit = 256;
std::unordered_map<std::string, float> g_wrapHeightCache;

std::string wrapHeightCacheKey(nxui::Font* font, const std::string& label, float labelWidth) {
    return std::to_string((std::uintptr_t)font)
         + "\n" + std::to_string(font ? font->revision() : 0)
         + "\n" + std::to_string((int)std::lround(labelWidth * 4.f))
         + "\n" + label;
}

class SettingsTabWidget final : public nxui::GlassBox {
public:
    explicit SettingsTabWidget(const std::string& text)
        : nxui::GlassBox(nxui::Axis::ROW) {
        setCornerRadius(18.f);
        setBorderWidth(1.f);
        setWireframeEnabled(false);

        m_label = std::make_shared<nxui::Label>(text);
        m_label->setHAlign(nxui::Label::HAlign::Left);
        m_label->setVAlign(nxui::Label::VAlign::Center);
        addChild(m_label);
    }

    void sync(const std::string& text,
              nxui::Font* font,
              const nxui::Theme* theme,
              bool selected,
              bool focused,
              float uiTime,
              float accentWidth) {
        (void)uiTime;
        m_selected = selected;
        m_focused = focused;
        m_accentWidth = accentWidth;
        m_accentColor = theme ? theme->cursorNormal : nxui::Color::white();

        if (font != m_cachedFont) {
            m_cachedFont = font;
            if (font) {
                m_label->setFont(font);
            }
        }
        if (m_cachedText != text) {
            m_cachedText = text;
            m_label->setText(m_cachedText);
        }

        float textScale = selected ? 0.91f : 0.87f;
        if (font && rect().width > 44.f) {
            const float measured = font->measure(text).x;
            if (measured > 0.f)
                textScale = std::min(textScale, (rect().width - 44.f) / measured);
            textScale = std::max(0.55f, textScale);
        }
        if (std::abs(m_cachedTextScale - textScale) > 0.001f) {
            m_cachedTextScale = textScale;
            m_label->setScale(textScale);
        }
        m_label->setOpacity(opacity());
        m_label->setRect({rect().x + 22.f, rect().y,
                          std::max(0.f, rect().width - 44.f), rect().height});

        if (theme) {
            nxui::Color textColor = selected ? theme->textPrimary : theme->textSecondary;
            nxui::Color baseColor = theme->panelBase.withAlpha(selected ? 0.94f : 0.88f);
            nxui::Color borderColor = selected
                ? theme->cursorNormal.withAlpha(focused ? 0.70f : 0.46f)
                : theme->panelBorder.withAlpha(focused ? 0.34f : 0.22f);
            nxui::Color hiColor = theme->panelHighlight.withAlpha(selected ? 0.12f : 0.06f);

            setBaseColor(baseColor);
            setBorderColor(borderColor);
            setHighlightColor(hiColor);
            setBorderWidth(selected || focused ? 1.2f : 1.f);
            setScale(selected ? 1.01f : 1.f);

            m_label->setTextColor(textColor);
        }
    }

    void syncTile(const std::string& text,
                  nxui::Font* font,
                  const nxui::Theme* theme,
                  bool selected,
                  bool focused,
                  const nxui::Color& accent) {
        (void)theme;
        m_tile = true;
        m_selected = selected;
        m_focused = focused;
        m_accentColor = accent;

        setBaseColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        setBorderWidth(0.f);
        setScale(1.f);

        if (font != m_cachedFont) {
            m_cachedFont = font;
            if (font) m_label->setFont(font);
        }
        if (m_cachedText != text) {
            m_cachedText = text;
            m_label->setText(m_cachedText);
        }
        m_label->setHAlign(nxui::Label::HAlign::Center);

        const nxui::Rect chip = settings::metrics::tileChipRect(rect(), selected);
        const float room = std::max(8.f, chip.width - 12.f);
        float textScale = selected ? 0.62f : 0.58f;
        if (font) {
            const float measured = font->measure(text).x;
            if (measured > 0.f)
                textScale = std::min(textScale, room / measured);
            textScale = std::max(0.38f, textScale);
        }
        if (std::abs(m_cachedTextScale - textScale) > 0.001f) {
            m_cachedTextScale = textScale;
            m_label->setScale(textScale);
        }
        m_label->setRect(chip);
        m_label->setOpacity(opacity());
        m_label->setTextColor(nxui::Color::white());
    }

protected:
    void onRender(nxui::Renderer& ren) override {
        if (m_tile) {
            renderTile(ren);
            return;
        }

        nxui::GlassBox::onRender(ren);

        if (!m_selected || opacity() <= 0.01f)
            return;

        nxui::Rect r = rect();
        float accentH = std::max(16.f, r.height * 0.46f);
        float accentY = r.y + (r.height - accentH) * 0.5f;
        float accentW = std::clamp(m_accentWidth, 2.f, std::max(2.f, r.width * 0.12f));
        nxui::Rect accent = {r.x + 8.f, accentY, accentW, accentH};
        ren.drawRoundedRect(accent, m_accentColor.withAlpha(0.92f * opacity()), 2.f);

        nxui::Rect underline = {r.x + 16.f, r.bottom() - 5.f, std::max(24.f, r.width - 32.f), 2.f};
        ren.drawRoundedRect(underline, m_accentColor.withAlpha(0.18f * opacity()), 1.f);
    }

private:
    void renderTile(nxui::Renderer& ren) {
        const float op = opacity();
        if (op <= 0.01f)
            return;

        const nxui::Rect chip = settings::metrics::tileChipRect(rect(), m_selected);
        const float radius = settings::metrics::kTileRadius;

        const nxui::Color fill = m_selected ? m_accentColor : m_dimTarget;

        ren.drawRoundedRect({chip.x, chip.y + (m_selected ? 4.f : 2.f), chip.width, chip.height},
                            nxui::Color(0.f, 0.f, 0.f, (m_selected ? 0.26f : 0.14f) * op),
                            radius);
        ren.drawRoundedRect(chip, fill.withAlpha((m_selected ? 1.f : 0.62f) * op), radius);
        ren.drawRoundedRect({chip.x + 3.f, chip.y + 2.f, chip.width - 6.f, chip.height * 0.42f},
                            nxui::Color::white().withAlpha(0.12f * op), radius * 0.7f);

        if (m_focused)
            ren.drawRoundedRectOutline(chip.expanded(3.f),
                                       nxui::Color::white().withAlpha(0.85f * op),
                                       radius + 3.f, 2.5f);

        if (m_selected)
            ren.drawRoundedRect({chip.x + (chip.width - 26.f) * 0.5f, chip.bottom() + 5.f,
                                 26.f, 4.f},
                                m_accentColor.withAlpha(0.95f * op), 2.f);
    }

    std::shared_ptr<nxui::Label> m_label;
    bool m_tile = false;
    nxui::Color m_dimTarget = nxui::Color(0.55f, 0.60f, 0.68f, 1.f);
    bool m_selected = false;
    bool m_focused = false;
    float m_accentWidth = 3.f;
    nxui::Color m_accentColor = nxui::Color::white();
    nxui::Font* m_cachedFont = nullptr;
    std::string m_cachedText;
    float m_cachedTextScale = -1.f;
};

class SettingsItemCard final : public nxui::GlassBox {
public:
    SettingsItemCard(TabbedOverlayScreen::SettingItem& item,
                     std::shared_ptr<nxui::Box> content)
        : nxui::GlassBox(nxui::Axis::ROW)
        , m_item(item)
        , m_content(std::move(content)) {
        setAlignItems(nxui::AlignItems::CENTER);
        setJustifyContent(nxui::JustifyContent::FLEX_START);
        setWireframeEnabled(false);
        m_depth = dynamic_cast<settings::widgets::DepthScalable*>(m_content.get());
        addChild(m_content);
    }

    void sync(const nxui::Theme* theme, bool selected, float alpha, float depthScale,
              const nxui::Color& accent, bool statTile) {
        if (m_depth) m_depth->setDepthScale(depthScale);
        const bool isSection = (m_item.type == TabbedOverlayScreen::ItemType::Section);
        const bool isActionLike = m_item.type == TabbedOverlayScreen::ItemType::Action
            || m_item.type == TabbedOverlayScreen::ItemType::Selector;
        m_isData = TabbedOverlayScreen::isDataItem(m_item.type);
        m_statTile = statTile && m_isData;
        m_accent = accent;

        setCornerRadius(isSection ? 14.f : m_statTile ? 16.f : m_isData ? 5.f : 18.f);
        setBorderWidth(isSection || m_isData ? 0.f : 1.f);
        setScale(selected ? 1.008f : 1.f);

        if (theme) {
            const float baseAlpha = isSection || m_isData ? 0.0f
                : selected ? 0.98f
                : isActionLike ? 0.92f : 0.90f;
            const float borderAlpha = isSection || m_isData ? 0.0f
                : selected ? 0.62f : 0.32f;
            const float hiAlpha = isSection || m_isData ? 0.0f
                : selected ? 0.14f : 0.07f;

            setBaseColor(theme->panelBase.withAlpha(baseAlpha));
            setBorderColor((selected ? accent : theme->panelBorder).withAlpha(borderAlpha));
            setHighlightColor(theme->panelHighlight.withAlpha(hiAlpha));

            m_recess = nxui::Color(0.f, 0.f, 0.f,
                                   theme->mode == nxui::ThemeMode::Dark ? 0.20f : 0.055f);
        }

        float insetX = isSection ? 6.f : 14.f;
        float insetY = isSection ? 4.f : 6.f;
        m_content->setRect({
            rect().x + insetX,
            rect().y + insetY,
            std::max(0.f, rect().width - insetX * 2.f),
            std::max(0.f, rect().height - insetY * 2.f)
        });
        m_content->setOpacity(alpha);
    }

protected:
    void onRender(nxui::Renderer& ren) override {
        if (m_isData && opacity() > 0.01f) {
            const nxui::Rect r = rect();
            const float op = opacity();
            ren.drawRoundedRect(r, m_recess.withAlpha(m_recess.a * op), cornerRadius());
            if (m_statTile) {
                ren.drawRoundedRectOutline(r, m_accent.withAlpha(0.30f * op),
                                           cornerRadius(), 1.5f);
                ren.drawRoundedRect({r.x + 10.f, r.y + 10.f, 3.f, r.height - 20.f},
                                    m_accent.withAlpha(0.80f * op), 1.5f);
            } else {
                ren.drawRect({r.x, r.y, 3.f, r.height}, m_accent.withAlpha(0.45f * op));
            }
        }
        nxui::GlassBox::onRender(ren);
    }

private:
    TabbedOverlayScreen::SettingItem& m_item;
    std::shared_ptr<nxui::Box> m_content;
    settings::widgets::DepthScalable* m_depth = nullptr;
    bool        m_isData = false;
    bool        m_statTile = false;
    nxui::Color m_recess { 0.f, 0.f, 0.f, 0.18f };
    nxui::Color m_accent = nxui::Color::white();
};

} // namespace


TabbedOverlayScreen::TabbedOverlayScreen(ScreenMode mode)
    : m_mode(mode) {
    setFrameworkTouchEnabled(false);
    setRect({kPanelMargin, kPanelMargin,
             1280.f - 2.f * kPanelMargin, 720.f - 2.f * kPanelMargin});
    setVisible(false);
    setOpacity(0.001f);
    setScale(0.92f);
    setCornerRadius(kPanelRadius);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setBlurEnabled(false);
    setBlurRadius(kSettingsBlurRadius);
    setBlurPasses(kSettingsBlurIter);
    setPanelOpacity(0.96f);

    m_focusCursor.setBorderWidth(2.6f);
    m_focusCursor.setCornerRadius(10.f);
    m_tabReveal.setImmediate(1.f);
    m_dropdownAnim.setImmediate(0.f);
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_contentSlideAnim.setImmediate(1.f);
    m_tabAccentW.setImmediate(3.f);

    m_tabBar = std::make_shared<nxui::GlassBox>(nxui::Axis::COLUMN);
    m_tabBar->setTag("tabBar");
    m_tabBar->setWireframeEnabled(false);

    m_tabContent = std::make_shared<nxui::GlassBox>(nxui::Axis::COLUMN);
    m_tabContent->setTag("tabContent");
    m_tabContent->setWireframeEnabled(false);

    rebuildTabBar();
    rebuildContentItems();

    m_i18nListenerId = nxui::I18n::instance().addLanguageChangedListener([this]() {
        m_deferredRefresh = true;
    });
}
TabbedOverlayScreen::~TabbedOverlayScreen() {
    nxui::I18n::instance().removeLanguageChangedListener(m_i18nListenerId);
}

float TabbedOverlayScreen::itemHeight(const SettingItem& item, float contentWidth) const {
    if (item.type == ItemType::Section) return kSectionHeight;
    if (!item.wrapLabel) return kRowHeight;

    const float labelWidth = std::max(1.f, contentWidth - 104.f);

    std::string key = wrapHeightCacheKey(m_font, item.label, labelWidth);
    auto cached = g_wrapHeightCache.find(key);
    if (cached != g_wrapHeightCache.end())
        return cached->second;

    nxui::Label probe(item.label);
    if (m_font) probe.setFont(m_font);
    probe.setScale(0.94f);
    probe.setMultiline(true);
    const float height = std::max(kRowHeight, probe.measureWrappedText(labelWidth).y + 34.f);

    if (g_wrapHeightCache.size() >= kWrapHeightCacheLimit)
        g_wrapHeightCache.clear();
    g_wrapHeightCache.emplace(std::move(key), height);
    return height;
}

void TabbedOverlayScreen::setTheme(const nxui::Theme* t) {
    m_theme = t;
    if (!m_theme)
        return;

    setBaseColor(m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark ? 0.95f : 0.96f));
    setBorderColor(m_theme->panelBorder.withAlpha(0.42f));
    setHighlightColor(m_theme->panelHighlight.withAlpha(0.10f));
    setLiquidGlassShade(m_theme->mode == nxui::ThemeMode::Dark ? 0.08f : -0.03f);
    invalidateBackdropCache();

    if (!usesCustomContentLayout()) {
        m_cachedTabContentWidgets.clear();
        m_cachedTabContentWidgets.resize(m_tabs.size());

        if (isActive())
            rebuildContentItems();
    }
}

void TabbedOverlayScreen::show() {
    if (m_active) return;
    DebugLog::log("[settings] show()");
    if (m_tabs.empty())
        warmup();

    const float margin = std::max(0.f, overlayPanelMargin());
    m_panelRadius = margin > 1.f ? kPanelRadius : 0.f;
    setRect({margin, margin, 1280.f - 2.f * margin, 720.f - 2.f * margin});
    setCornerRadius(m_panelRadius);
    invalidateBackdropCache();

    m_active    = true;
    m_animating = true;
    m_showing   = true;
    m_animT     = 0.f;
    m_focusArea  = FocusArea::Tabs;
    m_tabIndex   = 0;
    m_contentIdx = 0;
    m_scrollY    = 0.f;
    m_scrollTarget = 0.f;
    m_tabReveal.setImmediate(0.f);
    m_tabReveal.set(1.f, 0.24f, nxui::Easing::outCubic);
    m_dropdownOpen = false;
    m_dropdownClosing = false;
    m_dropdownRawIdx = -1;
    m_dropdownHover = 0;
    m_dropdownAnim.setImmediate(0.f);
    m_touchDirectControl = false;
    m_ignoreInitialTouchRelease = true;
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_contentSlideAnim.setImmediate(1.f);
    m_contentStaggerT = kContentStaggerDone;
    m_tabAccentW.setImmediate(3.f);
    if (m_tabBar) rebuildTabBar();
    if (m_tabContent) rebuildContentItems();
    invalidateBackdropCache();

    setVisible(true);
    syncPanelState(0.f);
    setFocusable(true);
    setupActions();
    announceCurrentFocus();
}

void TabbedOverlayScreen::hide() {
    if (!m_active) return;
    DebugLog::log("[settings] hide()");
    if (m_closeSfxCb) m_closeSfxCb();
    closeDropdown(false);
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_animating = true;
    m_showing   = false;
    m_animT     = 0.f;

    setFocusable(false);
    clearActions();
}

void TabbedOverlayScreen::openDropdown(int rawIdx) {
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size())
        return;

    auto& items = currentItems();
    if (rawIdx < 0 || rawIdx >= (int)items.size())
        return;

    auto& item = items[rawIdx];
    if (item.type != ItemType::Selector || item.options.empty())
        return;

    m_dropdownOpen = true;
    m_dropdownClosing = false;
    m_dropdownRawIdx = rawIdx;
    m_dropdownHover = std::clamp(item.intVal, 0, std::max(0, (int)item.options.size() - 1));
    int visible = std::min((int)item.options.size(), 6);
    m_dropdownVisualStart = (item.options.size() > (size_t)visible)
        ? (float)std::clamp(m_dropdownHover - visible / 2, 0, (int)item.options.size() - visible)
        : 0.f;
    m_dropdownAnim.set(1.f, 0.18f, nxui::Easing::outCubic);
}

void TabbedOverlayScreen::closeDropdown(bool animated) {
    m_dropdownOpen = false;
    m_dropdownClosing = animated && m_dropdownRawIdx >= 0;
    if (animated && m_dropdownRawIdx >= 0) {
        m_dropdownAnim.set(0.f, 0.16f, nxui::Easing::outCubic);
    } else {
        m_dropdownClosing = false;
        m_dropdownRawIdx = -1;
        m_dropdownAnim.setImmediate(0.f);
    }
}


void TabbedOverlayScreen::rebuildTabBar() {
    m_tabBar->clearChildren();
    invalidateLayout();
    const auto& L = layout();
    m_tabBar->setRect(L.tabs);

    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        auto tabBox = std::make_shared<SettingsTabWidget>(m_tabs[i].name);
        tabBox->setTag(m_tabs[i].name);
        tabBox->setRect(L.tabCards[(size_t)i]);
        m_tabBar->addChild(tabBox);
    }
}

void TabbedOverlayScreen::rebuildContentItems() {
    m_tabContent->clearChildren();
    invalidateLayout();
    m_tabContent->setRect(contentRect());

    if (usesCustomContentLayout())
        return;

    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;
    ensureTabLoaded(m_tabIndex);
    auto& items = currentItems();

    if (m_detailOpen) {
        const auto& L = layout();
        for (int i = 0; i < (int)items.size() && i < (int)L.rows.size(); ++i) {
            auto itemBox = makeItemWidget(items[(size_t)i]);
            itemBox->setTag(items[(size_t)i].label);
            itemBox->setRect(L.rows[(size_t)i].card);
            m_tabContent->addChild(itemBox);
        }
        return;
    }

    auto& cache = m_cachedTabContentWidgets[(size_t)m_tabIndex];

    if (cache.empty()) {
        invalidateLayout();
        const auto& L = layout();
        cache.reserve(items.size());

        for (int i = 0; i < (int)items.size(); ++i) {
            auto itemBox = makeItemWidget(items[i]);
            itemBox->setTag(items[i].label);
            itemBox->setRect(L.rows[(size_t)i].card);
            cache.push_back(itemBox);
        }
    }

    for (auto& itemBox : cache) {
        m_tabContent->addChild(itemBox);
    }
}

std::shared_ptr<nxui::Box> TabbedOverlayScreen::makeItemWidget(SettingItem& item) {
    settings::widgets::SettingWidgetContext ctx;
    ctx.font = &m_font;
    ctx.smallFont = &m_smallFont;
    ctx.theme = &m_theme;
    auto content = settings::widgets::createSettingItemWidget(item, ctx);
    return std::make_shared<SettingsItemCard>(item, content);
}

void TabbedOverlayScreen::onRender(nxui::Renderer& ren) {
    if (!m_active && !m_animating)
        return;

    float opacity = visibilityProgress();
    nxui::Rect p = panelRect(scale());

    if (m_theme)
        m_focusCursor.setColor(currentAccent());
    m_focusCursor.setOpacity(opacity);

    if (m_tabBar) m_tabBar->setRect(tabsRect(p));
    if (m_tabContent) m_tabContent->setRect(contentRect(p));

    const auto& tuning = settings::debug::settingsGlassTuning();
    if (ren.gpu().offscreenReady() &&
        (!m_backdropCacheValid ||
         std::abs(m_cachedPreBlurRadius - tuning.preBlurRadius) > 0.01f ||
         m_cachedBlurIterations != tuning.blurIterations)) {
        ren.captureToOffscreen(false);
        if (tuning.preBlurRadius > 0.01f && tuning.blurIterations > 0)
            ren.applyBlur(tuning.preBlurRadius, tuning.blurIterations);
        ren.copyOffscreen(0, 2);
        m_backdropCacheValid = true;
        m_cachedPreBlurRadius = tuning.preBlurRadius;
        m_cachedBlurIterations = tuning.blurIterations;
    }

    drawBackground(ren, p, opacity * 0.72f);

    if (opacity > 0.01f) {
        nxui::Color glassTint = m_theme
            ? m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark
                                               ? tuning.tintAlphaDark
                                               : tuning.tintAlphaLight)
            : m_base.withAlpha(tuning.tintAlphaDark);
        const float inset = isFullBleed() ? 0.f : std::max(0.0f, tuning.inset);
        nxui::Rect glassRect = p.shrunk(inset);
        float glassRadius = isFullBleed() ? 0.f
                                          : std::max(12.0f, m_panelRadius - inset * 0.5f);

        const nxui::Color border = m_theme
            ? m_theme->panelBorder.withAlpha(0.32f)
            : nxui::Color::white().withAlpha(0.18f);
        const nxui::Color highlight = m_theme
            ? m_theme->panelHighlight.withAlpha(0.11f)
            : nxui::Color::white().withAlpha(0.08f);
        if (!isFullBleed())
            ren.drawRoundedRect({glassRect.x, glassRect.y + 8.f,
                                 glassRect.width, glassRect.height},
                                nxui::Color(0.f, 0.f, 0.f, 0.20f * opacity),
                                glassRadius);
        if (m_backdropCacheValid) {
            const auto saved = ren.liquidGlassSettings();
            auto& glass = ren.liquidGlassSettings();
            glass.refractionIntensity = tuning.refractionIntensity;
            glass.blurIntensity = tuning.shaderBlurIntensity;
            glass.glowIntensity = tuning.glowIntensity;
            glass.saturation = tuning.saturation;
            glass.roughness = tuning.roughness;
            glass.powerFactor = tuning.powerFactor;
            ren.drawLiquidGlass(2, glassRect, glassRadius, glassTint,
                                opacity * 0.98f, tuning.shade);
            ren.liquidGlassSettings() = saved;
            if (!isFullBleed()) {
                ren.drawRoundedRectOutline(glassRect.shrunk(2.f),
                                           nxui::Color::white().withAlpha(0.18f * opacity),
                                           std::max(0.f, glassRadius - 2.f), 1.5f);
                ren.drawRoundedRectOutline(glassRect, border.withAlpha(0.48f * opacity),
                                           glassRadius, 1.5f);
            }
        } else {
            ren.drawFrostedInset(glassRect, glassTint, border, highlight,
                                 glassRadius, opacity);
        }
    }

    onContentRender(ren);

    if (ren.boxWireframeEnabled()) {
        syncDebugWireframeRects(p);
        if (m_tabBar) m_tabBar->render(ren);
        if (m_tabContent) m_tabContent->render(ren);
    }

    m_focusCursor.render(ren);
}

void TabbedOverlayScreen::onContentRender(nxui::Renderer& ren) {
    float opacity = visibilityProgress();
    nxui::Rect p = panelRect(scale());
    float textOp = m_showing ? opacity : opacity * opacity;

    ren.pushClipRect(p);
    drawOverlayHeader(ren, p, textOp);
    drawTabs(ren, p, textOp);
    drawContent(ren, p, textOp);
    drawDropdown(ren, p, textOp);
    drawTrackChangedToast(ren, p, textOp);
    ren.popClipRect();
}

void TabbedOverlayScreen::syncDebugWireframeRects(const nxui::Rect& panel) {
    if (!m_tabBar || !m_tabContent) return;

    const auto& L = layout();

    m_tabBar->setRect(L.tabs);
    auto& tabChildren = m_tabBar->children();
    for (int i = 0; i < (int)tabChildren.size() && i < (int)L.tabCards.size(); ++i)
        tabChildren[i]->setRect(L.tabCards[(size_t)i]);

    m_tabContent->setRect(L.content);
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;

    auto& itemChildren = m_tabContent->children();

    float slideT = std::clamp(m_contentSlideAnim.value(), 0.f, 1.f);
    float slideOffset = (1.f - slideT) * 24.f * (float)m_tabSwitchDir;

    int n = std::min((int)itemChildren.size(), (int)L.rows.size());
    for (int i = 0; i < n; ++i) {
        nxui::Rect card = L.rows[(size_t)i].card;
        card.y += slideOffset;
        itemChildren[i]->setRect(card);
    }
}

void TabbedOverlayScreen::drawBackground(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_theme || opacity <= 0.01f)
        return;

    nxui::Rect screen = {0.f, 0.f, (float)ren.width(), (float)ren.height()};
    nxui::Color scrim = nxui::Color::lerp(m_theme->background, nxui::Color::black(),
                                          m_theme->mode == nxui::ThemeMode::Dark ? 0.72f : 0.28f)
        .withAlpha((m_theme->mode == nxui::ThemeMode::Dark ? 0.14f : 0.10f) * opacity);

    ren.drawRect(screen, scrim);
}

void TabbedOverlayScreen::drawTabs(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_theme || !m_tabBar) return;
    nxui::Rect tr = tabsRect(panel);
    auto* tabPanel = static_cast<nxui::GlassBox*>(m_tabBar.get());

    const bool horizontal = usesHorizontalTabRail();

    tabPanel->setRect(tr);
    tabPanel->setOpacity(opacity);
    tabPanel->setCornerRadius(24.f);
    if (horizontal) {
        tabPanel->setBorderWidth(0.f);
        tabPanel->setBaseColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        tabPanel->setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        tabPanel->setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        tabPanel->setPanelOpacity(0.f);
    } else {
        tabPanel->setBorderWidth(1.f);
        tabPanel->setBaseColor(m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark ? 0.94f : 0.96f));
        tabPanel->setBorderColor(m_theme->panelBorder.withAlpha(0.30f));
        tabPanel->setHighlightColor(m_theme->panelHighlight.withAlpha(0.08f));
        tabPanel->setPanelOpacity(1.f);
    }

    const auto& L = layout();
    auto& tabChildren = m_tabBar->children();
    float reveal = std::clamp(m_tabReveal.value(), 0.f, 1.f);
    float rowOpacity = opacity * reveal;
    float rowYOffset = (1.f - reveal) * 6.f;

    int tabCount = std::min({(int)tabChildren.size(), (int)m_tabs.size(), (int)L.tabCards.size()});
    for (int i = 0; i < tabCount; ++i) {
        auto* tab = static_cast<SettingsTabWidget*>(tabChildren[i].get());
        nxui::Rect card = L.tabCards[(size_t)i];
        card.y += rowYOffset;  // reveal offset only; the resting rect is the layout's
        tab->setRect(card);
        tab->setOpacity(rowOpacity);
        const bool selected = (i == m_tabIndex);
        const bool focused  = (m_focusArea == FocusArea::Tabs && selected);
        if (horizontal) {
            tab->syncTile(m_tabs[i].name, m_font, m_theme, selected, focused, currentAccent());
        } else {
            tab->sync(m_tabs[i].name, m_font, m_theme, selected, focused,
                      m_uiTime, m_tabAccentW.value());
        }
    }

    if (m_focusArea == FocusArea::Tabs && m_tabIndex >= 0 && m_tabIndex < (int)tabChildren.size()) {
        const nxui::Rect card = tabChildren[m_tabIndex]->rect();
        const nxui::Rect target = horizontal
            ? settings::metrics::tileChipRect(card, true).expanded(5.f)
            : card.expanded(1.f);
        m_focusCursor.moveTo(target, horizontal ? settings::metrics::kTileRadius + 6.f : 16.f, 0.08f);
    }

    m_tabBar->render(ren);

    if (horizontal && tabCount > 0)
        drawRailArrows(ren, L, rowOpacity);
}

void TabbedOverlayScreen::drawRailArrows(nxui::Renderer& ren,
                                         const settings::SettingsLayout& L,
                                         float opacity) {
    if (!m_theme || opacity <= 0.01f || L.tabCards.empty())
        return;

    const nxui::Rect first = L.tabCards.front();
    const nxui::Rect last  = L.tabCards.back();
    const float size = 34.f;
    const float cy = L.tabs.y + settings::metrics::kRailTopPad
                   + settings::metrics::kTileCardH * 0.5f;

    auto arrow = [&](bool left) {
        const bool enabled = left ? (m_tabIndex > 0)
                                  : (m_tabIndex < (int)m_tabs.size() - 1);
        const float kick = m_railArrowKick[left ? 0 : 1];
        const float bump = kick * kick;

        const float gap = left ? (first.x - L.tabs.x) : (L.tabs.right() - last.right());
        if (gap < size + 10.f)
            return;

        const float cx = (left ? L.tabs.x + gap * 0.5f : L.tabs.right() - gap * 0.5f)
                       + (left ? -1.f : 1.f) * bump * 5.f;
        const float a  = (enabled ? 1.f : 0.30f) * opacity;
        const float r  = size * 0.5f * (1.f + 0.10f * bump);

        ren.drawCircle({cx, cy}, r, m_theme->panelBase.withAlpha(0.55f * a), 28);
        ren.drawCircle({cx, cy}, r - 1.5f, m_theme->panelBase.withAlpha(0.75f * a), 28);

        const float w = size * 0.20f;
        const float h = size * 0.26f;
        const nxui::Color ink = (enabled ? currentAccent() : m_theme->textSecondary)
                                    .withAlpha(0.95f * a);
        if (left)
            ren.drawTriangle({cx + w * 0.5f, cy - h}, {cx + w * 0.5f, cy + h},
                             {cx - w * 0.9f, cy}, ink);
        else
            ren.drawTriangle({cx - w * 0.5f, cy - h}, {cx - w * 0.5f, cy + h},
                             {cx + w * 0.9f, cy}, ink);
    };

    arrow(true);
    arrow(false);
}

void TabbedOverlayScreen::drawContent(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_smallFont || !m_theme) return;
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;
    if (!m_tabContent) return;

    nxui::Rect cr = contentRect(panel);
    auto* contentPanel = static_cast<nxui::GlassBox*>(m_tabContent.get());
    contentPanel->setRect(cr);
    contentPanel->setOpacity(opacity);
    contentPanel->setCornerRadius(26.f);
    contentPanel->setBorderWidth(1.f);
    contentPanel->setBaseColor(m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark ? 0.94f : 0.96f));
    contentPanel->setBorderColor(m_theme->panelBorder.withAlpha(0.30f));
    contentPanel->setHighlightColor(m_theme->panelHighlight.withAlpha(0.08f));
    contentPanel->setPanelOpacity(1.f);

    if (usesCustomContentLayout()) {
        contentPanel->render(ren);
        ren.pushClipRect(cr);
        drawCustomContent(ren, panel, cr, opacity);
        ren.popClipRect();
        return;
    }

    const auto& L = layout();
    auto& items = currentItems();
    auto& itemChildren = m_tabContent->children();
    int focusedRawIdx = (m_focusArea == FocusArea::Content && focusableCount() > 0)
        ? rawIndexFromFocusable(m_contentIdx) : -1;

    const float reveal = opacity * std::clamp(m_tabReveal.value(), 0.f, 1.f);
    const nxui::Color accent = currentAccent();

    int visibleRank = 0;
    int n = std::min({(int)itemChildren.size(), (int)items.size(), (int)L.rows.size()});
    for (int i = 0; i < n; ++i) {
        const settings::RowLayout& row = L.rows[(size_t)i];

        itemChildren[i]->setVisible(row.visible);
        if (!row.visible)
            continue;

        const float rowT = std::clamp(
            (m_contentStaggerT - (float)visibleRank * kRowStaggerDelay) / kRowRevealDur,
            0.f, 1.f);
        const float rowEase = nxui::Easing::outBack(rowT);
        const float rowFade = nxui::Easing::outCubic(rowT);
        ++visibleRank;

        nxui::Rect itemRect = row.card;
        if (L.statGrid) {
            const float s = 0.86f + 0.14f * rowEase;
            const float w = itemRect.width * s;
            const float h = itemRect.height * s;
            itemRect.x += (itemRect.width - w) * 0.5f;
            itemRect.y += (itemRect.height - h) * 0.5f;
            itemRect.width  = w;
            itemRect.height = h;
        } else {
            itemRect.y += (1.f - rowEase) * 18.f * (float)m_tabSwitchDir;
            itemRect.x += (1.f - rowEase) * kRowRevealSlideX;
        }
        const float rowOpacity = reveal * rowFade * row.depthAlpha;

        itemChildren[i]->setRect(itemRect);
        itemChildren[i]->setOpacity(rowOpacity);

        auto* card = static_cast<SettingsItemCard*>(itemChildren[i].get());
        bool selected = (i == focusedRawIdx);
        card->sync(m_theme, selected, rowOpacity, row.textScale, accent, L.statGrid);

        if (selected) {
            m_focusCursor.moveTo(itemChildren[i]->rect().expanded(1.f),
                                 items[i].type == ItemType::Section ? 14.f : 18.f,
                                 0.08f);
        }
    }

    ren.pushClipRect(cr);
    m_tabContent->render(ren);
    ren.popClipRect();

    float totalH = L.totalHeight + settings::metrics::kContentTopPad;
    if (totalH > cr.height + 1.f) {
        float maxScroll = std::max(1.f, L.maxScroll);
        float trackH = std::max(42.f, cr.height * std::clamp(cr.height / totalH, 0.12f, 1.f));
        float trackX = cr.right() - 8.f;
        float trackY = cr.y + 16.f;
        float trackAreaH = std::max(1.f, cr.height - 32.f);
        float thumbY = trackY + (trackAreaH - trackH) * std::clamp(m_scrollY / maxScroll, 0.f, 1.f);
        nxui::Rect rail = {trackX, trackY, 3.f, trackAreaH};
        nxui::Rect thumb = {trackX - 0.5f, thumbY, 4.f, trackH};
        ren.drawRoundedRect(rail, m_theme->panelBorder.withAlpha(0.16f * opacity), 1.5f);
        ren.drawRoundedRect(thumb, accent.withAlpha(0.46f * opacity), 2.f);
    }
}

void TabbedOverlayScreen::drawDropdown(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_theme || !m_smallFont || m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;

    float open = m_dropdownAnim.value();
    if (open <= 0.01f) return;

    auto& items = currentItems();
    if (m_dropdownRawIdx < 0 || m_dropdownRawIdx >= (int)items.size()) return;
    auto& item = items[m_dropdownRawIdx];
    if (item.type != ItemType::Selector || item.options.empty()) return;

    nxui::Rect cr = contentRect(panel);
    const auto& L = layout();
    if (L.dropdown.width <= 0.f) return;

    const int total = (int)item.options.size();
    const int visible = L.dropdownVisibleCount;
    const float optH = settings::metrics::kDropdownOptH;
    const float listH = L.dropdown.height;

    const float visualStart = L.dropdownVisualStart;
    int start = std::clamp((int)std::floor(visualStart), 0, std::max(0, total - visible));
    float rowOffset = (float)start - visualStart;

    float scale = 0.965f + 0.035f * open;
    float w = L.dropdown.width * scale;
    float h = listH * scale;
    float dx = L.dropdown.x + (L.dropdown.width - w) * 0.5f;
    float fy = L.dropdown.y + (listH - h) * 0.5f + (1.f - open) * 8.f;

    nxui::Rect pop = { dx, fy, w, h };

    float a = opacity * open;
    nxui::Color bg = m_theme->mode == nxui::ThemeMode::Dark
        ? nxui::Color::lerp(m_theme->panelBase, nxui::Color(0.055f, 0.060f, 0.075f, 1.f), 0.36f).withAlpha(0.95f * a)
        : nxui::Color::lerp(m_theme->panelBase, nxui::Color(0.98f, 0.985f, 1.f, 1.f), 0.30f).withAlpha(0.96f * a);
    float radius = 15.f;

    nxui::Rect contact = pop;
    contact.y += 6.f;
    ren.drawRoundedRect(contact.expanded(3.f), nxui::Color::black().withAlpha(0.12f * a), radius + 3.f);
    ren.drawRoundedRect(pop.expanded(1.f), m_theme->panelHighlight.withAlpha(0.08f * a), radius + 1.f);
    ren.drawRoundedRect(pop, bg, radius);
    ren.drawRoundedRectOutline(pop,
                               m_theme->panelBorder.withAlpha(0.70f * a),
                               radius, 1.4f);
    ren.drawRoundedRectOutline(pop.shrunk(2.f),
                               m_theme->panelHighlight.withAlpha(0.10f * a),
                               std::max(0.f, radius - 2.f), 1.f);

    nxui::Rect listClip = pop.shrunk(6.f);
    ren.pushClipRect(listClip);

    for (int i = 0; i < visible + 1; ++i) {
        int idx = start + i;
        if (idx < 0 || idx >= total)
            continue;
        float rowReveal = std::clamp((open - i * 0.025f) / 0.25f, 0.f, 1.f);
        float ry = listClip.y + 3.f + (rowOffset + (float)i) * optH + (1.f - rowReveal) * 5.f;
        nxui::Rect rr = { listClip.x + 3.f, ry, listClip.width - 6.f, optH - 2.f };
        if (rr.bottom() < listClip.y || rr.y > listClip.bottom())
            continue;

        bool hovered = idx == m_dropdownHover;
        bool active = idx == item.intVal;
        float rowAlpha = a * rowReveal;
        if (active) {
            ren.drawRoundedRect(rr,
                                m_theme->panelHighlight.withAlpha(0.065f * rowAlpha),
                                10.f);
            ren.drawRoundedRectOutline(rr.shrunk(1.f),
                                       m_theme->panelHighlight.withAlpha(0.075f * rowAlpha),
                                       9.f,
                                       1.f);
        }
        if (hovered) {
            float pulse = 0.9f + 0.1f * (std::sin(m_uiTime * 5.2f) * 0.5f + 0.5f);
            nxui::Color hi = m_theme->cursorNormal.withAlpha(0.20f * pulse * rowAlpha);
            ren.drawRoundedRect(rr, hi, 10.f);
            ren.drawRoundedRectOutline(rr,
                                       m_theme->cursorNormal.withAlpha(0.42f * rowAlpha),
                                       10.f,
                                       1.2f);
            if (m_dropdownOpen)
                m_focusCursor.moveTo(rr.shrunk(0.5f), 10.f, 0.08f);
        }

        nxui::Color tc = active ? m_theme->textPrimary : m_theme->textSecondary;
        std::string displayText = item.options[idx];
        float maxWidth = std::max(0.f, rr.width - 16.f);
        if (!displayText.empty()) {
            nxui::Vec2 tsz = m_smallFont->measure(displayText);
            if (tsz.x > maxWidth) {
                std::string ellipsis = "...";
                while (!displayText.empty()) {
                    displayText.pop_back();
                    tsz = m_smallFont->measure(displayText + ellipsis);
                    if (tsz.x <= maxWidth || displayText.empty())
                        break;
                }
                displayText += ellipsis;
            }
        }
        nxui::Vec2 tsz = m_smallFont->measure(displayText);
        float tx = rr.x + 14.f;
        float ty = rr.y + (rr.height - tsz.y * 0.84f) * 0.5f;
        ren.drawText(displayText, {tx, ty}, m_smallFont, tc.withAlpha(rowAlpha), 0.84f);
    }

    ren.popClipRect();

    if (total > visible) {
        float railH = std::max(1.f, pop.height - 24.f);
        float thumbH = std::max(24.f, railH * ((float)visible / (float)total));
        float maxStart = (float)std::max(1, total - visible);
        float thumbY = pop.y + 12.f + (railH - thumbH) * (visualStart / maxStart);
        nxui::Rect rail = {pop.right() - 10.f, pop.y + 12.f, 3.f, railH};
        nxui::Rect thumb = {pop.right() - 10.5f, thumbY, 4.f, thumbH};
        ren.drawRoundedRect(rail, m_theme->panelBorder.withAlpha(0.22f * a), 1.5f);
        ren.drawRoundedRect(thumb, m_theme->cursorNormal.withAlpha(0.56f * a), 2.f);
    }
}

void TabbedOverlayScreen::drawTrackChangedToast(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_smallFont || !m_theme) return;
    float t = m_trackToastAnim.value();
    if (t <= 0.01f || m_toastText.empty()) return;

    std::string displayText = m_toastText;
    float maxTextWidth = 420.f - 40.f;
    nxui::Vec2 tsz = m_smallFont->measure(displayText);
    if (tsz.x * 0.78f > maxTextWidth) {
        while (!displayText.empty() && m_smallFont->measure(displayText + "...").x * 0.78f > maxTextWidth)
            displayText.pop_back();
        displayText += "...";
        tsz = m_smallFont->measure(displayText);
    }

    float textWidth = std::min(maxTextWidth, tsz.x * 0.78f);
    float scale = 0.96f + 0.04f * t;
    float w = std::clamp(textWidth + 40.f, 220.f, 420.f) * scale;
    float h = 40.f * scale;
    float x = panel.right() - 24.f - w;
    float y = panel.y + 18.f;
    nxui::Rect r = {x, y, w, h};

    nxui::Color bg = (m_theme->mode == nxui::ThemeMode::Dark)
        ? nxui::Color(0.10f, 0.14f, 0.20f, 0.92f * t * opacity)
        : nxui::Color(0.90f, 0.95f, 1.00f, 0.94f * t * opacity);
    nxui::Color bd = m_theme->cursorNormal.withAlpha(0.65f * t * opacity);

    ren.drawRoundedRect(r, bg, 10.f);
    ren.drawRoundedRectOutline(r, bd, 10.f, 1.5f);

    float tx = r.x + 12.f;
    float ty = r.y + (r.height - tsz.y * 0.72f) * 0.5f;
    ren.drawText(displayText, {tx, ty}, m_smallFont, m_theme->textPrimary.withAlpha(t * opacity), 0.78f);
}
