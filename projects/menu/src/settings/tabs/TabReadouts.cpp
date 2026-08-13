#include "TabBuilders.hpp"

#include <utility>
#include <vector>

namespace settings::tabs {

namespace {
constexpr std::size_t kMinReadouts = 3;
}

void extractReadouts(SettingsScreen::Tab& tab,
                     SettingsScreen& screen,
                     const std::string& title,
                     const std::string& description) {
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;

    std::size_t readouts = 0;
    for (const auto& item : tab.items)
        if (item.type == ItemType::Info)
            ++readouts;
    if (readouts < kMinReadouts)
        return;

    std::vector<SettingItem> page;
    std::vector<SettingItem> kept;
    page.reserve(readouts + 1);
    kept.reserve(tab.items.size() - readouts + 1);

    for (auto& item : tab.items) {
        if (item.type == ItemType::Info)
            page.push_back(std::move(item));
        else
            kept.push_back(std::move(item));
    }

    for (std::size_t i = kept.size(); i-- > 0; ) {
        if (kept[i].type != ItemType::Section)
            continue;
        const bool headsSomething = (i + 1 < kept.size())
                                 && kept[i + 1].type != ItemType::Section;
        if (!headsSomething)
            kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(i));
    }

    SettingItem header;
    header.label = title;
    header.type  = ItemType::Section;
    page.insert(page.begin(), std::move(header));

    SettingItem opener;
    opener.label = title;
    opener.description = description;
    opener.type = ItemType::Action;
    opener.onChange = [&screen, page](SettingItem&) mutable {
        screen.openDetailPage(page);
    };
    kept.insert(kept.begin(), std::move(opener));

    tab.items = std::move(kept);
}

} // namespace settings::tabs
