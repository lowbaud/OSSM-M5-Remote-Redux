#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "devices/ossm/OssmClient.h"
#include "platform/RemoteInput.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

enum class DiagnosticsScreenAction : std::uint8_t {
    None,
    Back,
};

// Shows system information, and the diagnostic log in a second view on the same screen. The
// EEZ screen only provides the panel and buttons; the content is built here.
class DiagnosticsScreen {
  public:
    explicit DiagnosticsScreen(ossm::OssmClient& client);

    void begin();
    void enter();
    void leave();
    DiagnosticsScreenAction update(const RemoteInputEvents& events);
    void setStopAvailable(bool available);
    void setMotionActive(bool active);
    void setOssmName(const char* name);

    void requestBack();
    void requestAction();

  private:
    enum class View : std::uint8_t {
        Overview,
        Log,
    };

    enum OverviewRow : std::size_t {
        kFirmwareRow,
        kTargetRow,
        kUptimeRow,
        kResetRow,
        kMemoryRow,
        kLargestBlockRow,
        kPsramRow,
        kBatteryRow,
        kChargeCurrentRow,
        kOssmRow,
        kSignalRow,
        kRemoteAddressRow,
        kWorkerStackRow,
        kLogRow,
        kOverviewRowCount,
    };

    static constexpr std::size_t kMaxLogColumns = 60;
    static constexpr std::size_t kMaxLogRows = 24;
    static constexpr std::size_t kOssmNameCapacity = 32;

    ossm::OssmClient& client_;
    View view_ = View::Overview;
    DiagnosticsScreenAction pendingAction_ = DiagnosticsScreenAction::None;
    StopButtonFeedback stopButtonFeedback_;
    lv_obj_t* actionLabel_ = nullptr;

    lv_obj_t* overview_ = nullptr;
    std::array<lv_obj_t*, kOverviewRowCount> overviewValues_{};
    std::uint32_t lastOverviewRefreshAtMs_ = 0;
    char ossmName_[kOssmNameCapacity] = {};

    lv_obj_t* logView_ = nullptr;
    lv_obj_t* logLabel_ = nullptr;
    lv_obj_t* logStatusLabel_ = nullptr;
    std::size_t logColumns_ = 0;
    std::size_t logRows_ = 0;
    // Display lines scrolled back from the newest line.
    std::size_t logScrollLines_ = 0;
    std::size_t logTotalLines_ = 0;
    std::uint32_t renderedLogEnd_ = 0;
    char logLines_[kMaxLogRows][kMaxLogColumns + 1] = {};
    char logText_[kMaxLogRows * (kMaxLogColumns + 1) + 1] = {};

    void buildOverview(lv_obj_t* panel);
    void buildLogView(lv_obj_t* panel);
    void showView(View view);
    void refreshOverview();
    void renderLog();
    void scrollLog(std::int32_t linesBack);
    void updateLogStatus(bool newEntriesBelow);
};

}  // namespace m5_redux
