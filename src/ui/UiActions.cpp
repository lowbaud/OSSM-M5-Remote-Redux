#include "app/App.h"
#include "ui/generated/actions.h"

extern "C" void action_welcome_scan(lv_event_t*) {
    m5_redux::app::showScan();
}

extern "C" void action_welcome_settings(lv_event_t*) {
    m5_redux::app::showMenu();
}

extern "C" void action_scan_cancel(lv_event_t*) {
    m5_redux::app::cancelScan();
}

extern "C" void action_scan_connect(lv_event_t*) {
    m5_redux::app::connectSelectedScanDevice();
}

extern "C" void action_connect_cancel(lv_event_t*) {
    m5_redux::app::cancelConnection();
}

extern "C" void action_stop(lv_event_t*) {
    m5_redux::app::stopMotion();
}

extern "C" void action_ossm_control_patterns(lv_event_t*) {
    m5_redux::app::showOssmPatterns();
}

extern "C" void action_ossm_control_settings(lv_event_t*) {
    m5_redux::app::showMenu();
}

extern "C" void action_ossm_patterns_cancel(lv_event_t*) {
    m5_redux::app::cancelOssmPatterns();
}

extern "C" void action_ossm_patterns_select(lv_event_t*) {
    m5_redux::app::selectOssmPattern();
}

// The EEZ settings screen is no longer shown; MenuScreen replaced it. Remove these together with
// the screen in the EEZ project.
extern "C" void action_settings_back(lv_event_t*) {}

extern "C" void action_settings_select(lv_event_t*) {}

extern "C" void action_diagnostics_back(lv_event_t*) {
    m5_redux::app::activateDiagnosticsBack();
}

extern "C" void action_diagnostics_action(lv_event_t*) {
    m5_redux::app::activateDiagnosticsAction();
}
