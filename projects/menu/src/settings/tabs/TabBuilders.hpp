#pragma once

#include "../SettingsScreen.hpp"

namespace settings::tabs {

// Moves read-only rows onto a detail page behind one action; no-op below three.
// Never call it when a callback holds a reference into tab.items: it would dangle.
void extractReadouts(SettingsScreen::Tab& tab,
                     SettingsScreen& screen,
                     const std::string& title,
                     const std::string& description);

class SystemTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class AccessibilityTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class AudioTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class DisplayTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class InternetTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class ControllersTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class BluetoothTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class SleepTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class StorageTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

class AboutTab {
public:
    static SettingsScreen::Tab build(SettingsScreen& screen);
};

}
