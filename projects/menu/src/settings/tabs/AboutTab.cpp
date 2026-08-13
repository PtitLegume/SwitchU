#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>

#ifndef SWITCHU_VERSION
#define SWITCHU_VERSION "unknown"
#endif

namespace {

std::vector<SettingsScreen::SettingItem> buildCreditsPage() {
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    std::vector<SettingItem> page;

    auto section = [&](const std::string& key, const std::string& fallback) {
        SettingItem it;
        it.label = i18n.tr(key, fallback);
        it.type  = ItemType::Section;
        page.push_back(std::move(it));
    };
    auto entry = [&](const std::string& label, const std::string& value) {
        SettingItem it;
        it.label = label;
        it.type  = ItemType::Info;
        it.infoText = value;
        page.push_back(std::move(it));
    };

    section("settings.about.thanks", "Thanks");
    entry("PoloNX", i18n.tr("settings.about.role_author", "Author"));
    entry(i18n.tr("settings.about.contributors", "Contributors"),
          i18n.tr("settings.about.contributors_value", "github.com/PoloNX/SwitchU"));
    entry(i18n.tr("settings.about.translators", "Translators"),
          i18n.tr("settings.about.translators_value", "8 languages"));

    section("settings.about.inspiration", "Inspiration");
    entry("Nintendo Wii U", i18n.tr("settings.about.inspiration_value", "HOME Menu"));

    return page;
}

} // namespace

SettingsScreen::Tab settings::tabs::AboutTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.about", "About");

    // Application info
    {
        SettingItem it;
        it.label = i18n.tr("settings.about.version", "Version");
        it.type  = ItemType::Info;
        it.infoText = "SwitchU " SWITCHU_VERSION;
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.about.author", "Author");
        it.type  = ItemType::Info;
        it.infoText = "PoloNX";
        t.items.push_back(std::move(it));
    }

    // License
    {
        SettingItem it;
        it.label = i18n.tr("settings.about.license", "License");
        it.type  = ItemType::Info;
        it.infoText = i18n.tr("settings.about.license_value", "GPL-2.0");
        t.items.push_back(std::move(it));
    }

    // Source code
    {
        SettingItem it;
        it.label = i18n.tr("settings.about.source_code", "Source Code");
        it.type  = ItemType::Info;
        it.infoText = i18n.tr("settings.about.source_code_value", "github.com/PoloNX/SwitchU");
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.about.acknowledgements", "Acknowledgements");
        it.description = i18n.tr("settings.about.acknowledgements_desc",
                                 "The people behind this menu.");
        it.type  = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            screen.openDetailPage(buildCreditsPage());
        };
        t.items.push_back(std::move(it));
    }

    return t;
}
