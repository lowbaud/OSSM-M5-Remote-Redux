#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "platform/RemoteInput.h"
#include "settings/SettingsStore.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

enum class SettingsScreenAction : std::uint8_t {
    None,
    Back,
    PreviewBrightness,
    RestoreBrightness,
    CommitBrightness,
    CommitIdleDimTimeout,
    CommitIdlePowerOffTimeout,
    CommitAutoConnect,
};

struct SettingsScreenEvent {
    SettingsScreenAction action = SettingsScreenAction::None;
    BrightnessLevel brightnessLevel = SettingsStore::kDefaultBrightnessLevel;
    IdleDimTimeout idleDimTimeout = SettingsStore::kDefaultIdleDimTimeout;
    IdlePowerOffTimeout idlePowerOffTimeout = SettingsStore::kDefaultIdlePowerOffTimeout;
    bool autoConnectEnabled = SettingsStore::kDefaultAutoConnectEnabled;
};

class SettingsScreen {
  public:
    explicit SettingsScreen(SettingsStore& settings);

    void begin();
    void enter();
    void leave();
    SettingsScreenEvent update(const RemoteInputEvents& events);
    void refresh();
    void setStopAvailable(bool available);
    void setMotionActive(bool active);

    void requestBack();
    void requestSelect();
    void commitSucceeded();
    void commitFailed();

  private:
    struct SettingRow {
        lv_obj_t* button = nullptr;
        lv_obj_t* valueLabel = nullptr;
    };

    struct OptionRow {
        lv_obj_t* button = nullptr;
        lv_obj_t* checkLabel = nullptr;
        lv_obj_t* textLabel = nullptr;
    };

    static constexpr std::size_t kSettingCount = 4;
    static constexpr std::size_t kMaxOptionRows = 4;
    static constexpr std::size_t kNoSelection = static_cast<std::size_t>(-1);
    static_assert(
        kMaxOptionRows >= SettingsStore::kBrightnessOptionCount,
        "Settings option pool is too small");
    static_assert(
        kMaxOptionRows >= SettingsStore::kIdleDimOptionCount, "Settings option pool is too small");
    static_assert(
        kMaxOptionRows >= SettingsStore::kIdlePowerOffOptionCount,
        "Settings option pool is too small");
    static_assert(
        kMaxOptionRows >= SettingsStore::kAutoConnectOptionCount,
        "Settings option pool is too small");

    SettingsStore& settings_;
    std::array<SettingRow, kSettingCount> settingRows_{};
    std::array<OptionRow, kMaxOptionRows> optionRows_{};
    lv_obj_t* optionsTitle_ = nullptr;
    lv_obj_t* optionsList_ = nullptr;
    std::size_t selectedSettingIndex_ = 0;
    std::size_t selectedOptionIndex_ = kNoSelection;
    SettingsScreenEvent pendingEvent_{};
    StopButtonFeedback stopButtonFeedback_;
    bool stopAvailable_ = false;
    bool optionsOpen_ = false;

    void buildSettingRows();
    void buildOptionsPanel();
    void configureOptions();
    void openSelectedSetting();
    void closeOptions();
    std::size_t currentOptionCount() const;
    std::size_t currentStoredOptionIndex() const;
    void selectOption(std::size_t index, bool preview);
    void selectSetting(std::size_t index);
    void handleSettingClicked(lv_obj_t* row);
    void handleOptionClicked(lv_obj_t* row);

    static void handleSettingRowEvent(lv_event_t* event);
    static void handleOptionRowEvent(lv_event_t* event);
};

}  // namespace m5_redux
