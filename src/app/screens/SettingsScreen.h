#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "devices/ossm/OssmClient.h"
#include "platform/RemoteInput.h"
#include "settings/SettingsStore.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

enum class SettingsScreenAction : std::uint8_t {
    None,
    Back,
    PreviewBrightness,
    RestoreBrightness,
    CommitDefaultPattern,
    CommitBrightness,
    CommitIdleDimTimeout,
    CommitIdlePowerOffTimeout,
    CommitAutoConnect,
    CommitTouchscreen,
    CommitDepthControl,
    CommitStrokeDirection,
    ShowDiagnostics,
    Disconnect,
};

struct SettingsScreenEvent {
    SettingsScreenAction action = SettingsScreenAction::None;
    std::string defaultPattern;
    BrightnessLevel brightnessLevel = SettingsStore::kDefaultBrightnessLevel;
    IdleDimTimeout idleDimTimeout = SettingsStore::kDefaultIdleDimTimeout;
    IdlePowerOffTimeout idlePowerOffTimeout = SettingsStore::kDefaultIdlePowerOffTimeout;
    bool autoConnectEnabled = SettingsStore::kDefaultAutoConnectEnabled;
    bool touchscreenEnabled = SettingsStore::kDefaultTouchscreenEnabled;
    DepthControlMode depthControlMode = SettingsStore::kDefaultDepthControlMode;
    bool strokeEncoderReversed = SettingsStore::kDefaultStrokeEncoderReversed;
};

class SettingsScreen {
  public:
    explicit SettingsScreen(SettingsStore& settings);

    void begin();
    // Keeping the selection returns to the row that was selected when the screen was left.
    void enter(bool keepSelection = false);
    void setPatternCatalog(const ossm::OssmClient::PatternList& catalog);
    void leave();
    SettingsScreenEvent update(const RemoteInputEvents& events);
    void refresh();
    void setStopAvailable(bool available);
    void setDisconnectAvailable(bool available);
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

    static constexpr std::size_t kSettingCount = 10;
    static constexpr std::size_t kMaxOptionRows = ossm::OssmClient::kMaxPatternCount + 1;
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
    static_assert(
        kMaxOptionRows >= SettingsStore::kTouchscreenOptionCount,
        "Settings option pool is too small");
    static_assert(
        kMaxOptionRows >= SettingsStore::kDepthControlOptionCount,
        "Settings option pool is too small");
    static_assert(
        kMaxOptionRows >= SettingsStore::kStrokeDirectionOptionCount,
        "Settings option pool is too small");

    SettingsStore& settings_;
    ossm::OssmClient::PatternList patternCatalog_{};
    std::size_t defaultPatternOptionIndex() const;
    std::array<SettingRow, kSettingCount> settingRows_{};
    std::array<OptionRow, kMaxOptionRows> optionRows_{};
    lv_obj_t* optionsTitle_ = nullptr;
    lv_obj_t* optionsList_ = nullptr;
    lv_obj_t* saveFailureLabel_ = nullptr;
    std::uint32_t saveFailureShownAtMs_ = 0;
    std::size_t selectedSettingIndex_ = 0;
    std::size_t selectedOptionIndex_ = kNoSelection;
    SettingsScreenEvent pendingEvent_{};
    StopButtonFeedback stopButtonFeedback_;
    bool stopAvailable_ = false;
    bool disconnectAvailable_ = false;
    bool optionsOpen_ = false;

    void buildSettingRows();
    void addSettingRow(std::size_t index, const char* name, const char* value);
    void buildOptionsPanel();
    void configureOptions();
    // Shows the setting name in uppercase as the options panel heading.
    void setOptionsTitle(const char* title);
    void openSelectedSetting();
    // Fits the panel to the current option count so short lists leave no empty space.
    void resizeOptionsPanel();
    void closeOptions();
    void clearSaveFailure();
    std::size_t currentOptionCount() const;
    std::size_t currentStoredOptionIndex() const;
    void selectOption(std::size_t index, bool preview);
    void selectSetting(std::size_t index);
    bool settingVisible(std::size_t index) const;
    std::size_t defaultSettingIndex() const;
    // Moves the selection by visible rows, stopping at the first and last one.
    std::size_t stepSetting(std::int64_t steps) const;
    void handleSettingClicked(lv_obj_t* row);
    void handleOptionClicked(lv_obj_t* row);

    static void handleSettingRowEvent(lv_event_t* event);
    static void handleOptionRowEvent(lv_event_t* event);
};

}  // namespace m5_redux
