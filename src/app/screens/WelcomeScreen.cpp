#include "WelcomeScreen.h"

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

void WelcomeScreen::begin() {
    batteryIndicator_.begin(objects.welcome_battery_lbl);
}

void WelcomeScreen::enter() {
    if (lv_screen_active() != objects.welcome) {
        loadScreen(SCREEN_ID_WELCOME);
    }
}

void WelcomeScreen::leave() {}

WelcomeScreenAction WelcomeScreen::update(const RemoteInputEvents& events) {
    if (events.leftClick) {
        return WelcomeScreenAction::Settings;
    }
    if (events.rightClick) {
        return WelcomeScreenAction::Scan;
    }
    return WelcomeScreenAction::None;
}

void WelcomeScreen::setBatteryLevel(int percent) {
    batteryIndicator_.setLevel(percent);
}

}  // namespace m5_redux
