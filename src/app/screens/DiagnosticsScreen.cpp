#include "DiagnosticsScreen.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/BuildInfo.h"
#include "diagnostics/Log.h"
#include "diagnostics/LogBuffer.h"
#include "diagnostics/SystemInfo.h"
#include "platform/M5Platform.h"
#include "ui/generated/fonts.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kOverviewPadHorizontal = 10;
constexpr std::int32_t kOverviewPadVertical = 6;
constexpr std::int32_t kOverviewRowPad = 3;
constexpr std::int32_t kOverviewKeyWidth = 112;
// About two rows per detent. Scrolling jumps instead of animating, because each animation
// frame redraws the whole panel and the redraw is visible.
constexpr std::int32_t kOverviewScrollStep = 44;
constexpr std::uint32_t kOverviewRefreshIntervalMs = 1000;
constexpr std::int32_t kLogPad = 6;
// Some glyphs fill the whole line height, so keep a pixel between lines.
constexpr std::int32_t kLogLineSpace = 1;
constexpr std::int32_t kLogStatusGap = 2;
constexpr std::int32_t kLogScrollStep = 3;
constexpr std::size_t kContinuationIndent = 2;
constexpr std::size_t kMaxLinesPerRecord = 8;
constexpr std::size_t kWrapLineCapacity = 64;
constexpr char kPreviousBootSeparator[] = "-- previous boot --";
constexpr char kCurrentBootSeparator[] = "-- current boot --";

constexpr const char* kOverviewKeys[] = {
    "Firmware",
    "Target",
    "Uptime",
    "Last reset",
    "Memory",
    "Largest block",
    "PSRAM",
    "Battery",
    "OSSM",
    "Signal",
    "Remote address",
    "Worker stack",
    "Log",
};

void formatUptime(char* out, std::size_t capacity, std::uint32_t uptimeMs) {
    const unsigned long seconds = uptimeMs / 1000;
    const unsigned long hours = seconds / 3600;
    const unsigned long minutes = (seconds / 60) % 60;
    if (hours > 0) {
        std::snprintf(out, capacity, "%luh %02lum %02lus", hours, minutes, seconds % 60);
    } else if (minutes > 0) {
        std::snprintf(out, capacity, "%lum %02lus", minutes, seconds % 60);
    } else {
        std::snprintf(out, capacity, "%lus", seconds);
    }
}

const char* firmwareFamilyName(ossm::OssmClient::FirmwareFamily family) {
    switch (family) {
        case ossm::OssmClient::FirmwareFamily::Official:
            return "official";
        case ossm::OssmClient::FirmwareFamily::Lite:
            return "Lite";
        case ossm::OssmClient::FirmwareFamily::Unknown:
            break;
    }

    return "unidentified";
}

const char* plural(unsigned long count) {
    return count == 1 ? "" : "s";
}

// Splits one record into display lines, preferring to break at spaces. Continuation lines
// are indented.
std::size_t wrapRecord(
    const logging::LogRecord& record,
    std::size_t columns,
    char (&lines)[kMaxLinesPerRecord][kWrapLineCapacity]) {
    char text[32 + logging::LogRecord::kTagCapacity + logging::LogRecord::kMessageCapacity];
    std::snprintf(
        text,
        sizeof(text),
        "%lu.%03lu %c %s %s",
        static_cast<unsigned long>(record.timeMs / 1000),
        static_cast<unsigned long>(record.timeMs % 1000),
        logging::levelLetter(record.level),
        record.tag,
        record.message);

    columns = std::min(columns, kWrapLineCapacity - 1);
    std::size_t count = 0;
    const char* remaining = text;
    while (*remaining != '\0' && count < kMaxLinesPerRecord) {
        const std::size_t indent = count == 0 ? 0 : kContinuationIndent;
        const std::size_t available = columns > indent ? columns - indent : 1;
        const std::size_t length = std::strlen(remaining);
        std::size_t take = std::min(length, available);
        if (length > available) {
            std::size_t breakAt = take;
            while (breakAt > available / 2 && remaining[breakAt] != ' ') {
                --breakAt;
            }
            if (remaining[breakAt] == ' ') {
                take = breakAt;
            }
        }

        char* line = lines[count++];
        std::memset(line, ' ', indent);
        std::memcpy(line + indent, remaining, take);
        line[indent + take] = '\0';

        remaining += take;
        while (*remaining == ' ') {
            ++remaining;
        }
    }

    return count;
}

}  // namespace

DiagnosticsScreen::DiagnosticsScreen(ossm::OssmClient& client) : client_(client) {}

void DiagnosticsScreen::begin() {
    stopButtonFeedback_.begin(objects.diagnostics_stop_btn);
    setStopAvailable(false);
    actionLabel_ = lv_obj_get_child(objects.diagnostics_action_btn, 0);

    lv_obj_t* panel = objects.diagnostics_panel;
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);

    buildOverview(panel);
    buildLogView(panel);
    showView(View::Overview);
}

void DiagnosticsScreen::enter() {
    pendingAction_ = DiagnosticsScreenAction::None;
    showView(View::Overview);
    lv_obj_scroll_to_y(overview_, 0, LV_ANIM_OFF);

    if (lv_screen_active() != objects.diagnostics) {
        loadScreen(SCREEN_ID_DIAGNOSTICS);
    }
}

void DiagnosticsScreen::leave() {
    pendingAction_ = DiagnosticsScreenAction::None;
    stopButtonFeedback_.reset();
}

DiagnosticsScreenAction DiagnosticsScreen::update(const RemoteInputEvents& events) {
    const std::int32_t steps = events.encoderSteps[3];
    if (view_ == View::Overview) {
        if (steps != 0) {
            lv_obj_scroll_by_bounded(overview_, 0, -steps * kOverviewScrollStep, LV_ANIM_OFF);
        }
        if (millis() - lastOverviewRefreshAtMs_ >= kOverviewRefreshIntervalMs) {
            refreshOverview();
        }
    } else if (steps != 0) {
        // Turning forward moves towards newer lines, like moving down a list.
        scrollLog(-steps * kLogScrollStep);
    } else {
        std::uint32_t first = 0;
        std::uint32_t end = 0;
        logging::recordRange(first, end);
        if (end != renderedLogEnd_) {
            if (logScrollLines_ == 0) {
                renderLog();
            } else {
                updateLogStatus(true);
            }
        }
    }

    if (events.rightClick) {
        requestAction();
    }
    if (events.leftClick) {
        requestBack();
    }

    const DiagnosticsScreenAction action = pendingAction_;
    pendingAction_ = DiagnosticsScreenAction::None;
    return action;
}

void DiagnosticsScreen::setStopAvailable(bool available) {
    if (available) {
        lv_obj_remove_flag(objects.diagnostics_stop_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(objects.diagnostics_stop_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void DiagnosticsScreen::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void DiagnosticsScreen::setOssmName(const char* name) {
    std::snprintf(ossmName_, sizeof(ossmName_), "%s", name ? name : "");
}

void DiagnosticsScreen::requestBack() {
    if (view_ == View::Log) {
        showView(View::Overview);
    } else {
        pendingAction_ = DiagnosticsScreenAction::Back;
    }
}

void DiagnosticsScreen::requestAction() {
    if (view_ == View::Overview) {
        logScrollLines_ = 0;
        showView(View::Log);
    } else if (logScrollLines_ != 0) {
        logScrollLines_ = 0;
        renderLog();
    }
}

void DiagnosticsScreen::buildOverview(lv_obj_t* panel) {
    overview_ = lv_obj_create(panel);
    lv_obj_remove_style_all(overview_);
    lv_obj_set_size(overview_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(overview_, kOverviewPadHorizontal, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(overview_, kOverviewPadVertical, LV_PART_MAIN);
    lv_obj_set_flex_flow(overview_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(overview_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(overview_, LV_SCROLLBAR_MODE_AUTO);

    for (std::size_t index = 0; index < kOverviewRowCount; ++index) {
        lv_obj_t* row = lv_obj_create(overview_);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, kOverviewRowPad, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        // Let drags on a row scroll the overview.
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* key = lv_label_create(row);
        lv_obj_set_width(key, kOverviewKeyWidth);
        lv_obj_set_style_text_opa(key, LV_OPA_70, LV_PART_MAIN);
        lv_label_set_text_static(key, kOverviewKeys[index]);

        lv_obj_t* value = lv_label_create(row);
        lv_obj_set_flex_grow(value, 1);
        lv_label_set_long_mode(value, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text_static(value, "");
        overviewValues_[index] = value;
    }

    lv_label_set_text_static(overviewValues_[kFirmwareRow], buildInfo().buildVersion);
    lv_label_set_text_static(overviewValues_[kTargetRow], BUILD_TARGET);
    lv_label_set_text_static(overviewValues_[kResetRow], system_info::resetReasonName());
    if (system_info::resetWasAbnormal()) {
        lv_obj_set_style_text_color(
            overviewValues_[kResetRow], lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    }
}

void DiagnosticsScreen::buildLogView(lv_obj_t* panel) {
    logView_ = lv_obj_create(panel);
    lv_obj_remove_style_all(logView_);
    lv_obj_set_size(logView_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(logView_, kLogPad, LV_PART_MAIN);
    lv_obj_remove_flag(logView_, LV_OBJ_FLAG_SCROLLABLE);

    // Monospaced, so the text grid below maps exactly onto the label.
    const lv_font_t* logFont = &ui_font_dos_v_jpn12;

    logLabel_ = lv_label_create(logView_);
    lv_obj_set_style_text_font(logLabel_, logFont, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(logLabel_, kLogLineSpace, LV_PART_MAIN);
    lv_label_set_long_mode(logLabel_, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text_static(logLabel_, "");

    const lv_font_t* statusFont = &lv_font_montserrat_12;
    logStatusLabel_ = lv_label_create(logView_);
    lv_obj_set_style_text_font(logStatusLabel_, statusFont, LV_PART_MAIN);
    lv_obj_set_style_text_opa(logStatusLabel_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_align(logStatusLabel_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text_static(logStatusLabel_, "");

    lv_obj_update_layout(logView_);
    const std::int32_t contentWidth = lv_obj_get_content_width(logView_);
    const std::int32_t contentHeight = lv_obj_get_content_height(logView_);
    const std::int32_t logHeight =
        contentHeight - lv_font_get_line_height(statusFont) - kLogStatusGap;
    lv_obj_set_pos(logLabel_, 0, 0);
    lv_obj_set_size(logLabel_, contentWidth, logHeight);

    const std::int32_t glyphWidth =
        std::max<std::int32_t>(1, lv_font_get_glyph_width(logFont, 'M', 0));
    const std::int32_t lineHeight = lv_font_get_line_height(logFont) + kLogLineSpace;
    logColumns_ = std::min(kMaxLogColumns, static_cast<std::size_t>(contentWidth / glyphWidth));
    logRows_ =
        std::min(kMaxLogRows, static_cast<std::size_t>((logHeight + kLogLineSpace) / lineHeight));
}

void DiagnosticsScreen::showView(View view) {
    view_ = view;
    if (view == View::Overview) {
        lv_obj_add_flag(logView_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(overview_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_static(actionLabel_, "Log");
        lv_obj_remove_state(objects.diagnostics_action_btn, LV_STATE_DISABLED);
        refreshOverview();
    } else {
        lv_obj_add_flag(overview_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(logView_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_static(actionLabel_, "Newest");
        renderLog();
    }
}

void DiagnosticsScreen::refreshOverview() {
    lastOverviewRefreshAtMs_ = millis();
    char text[64];

    formatUptime(text, sizeof(text), millis());
    lv_label_set_text(overviewValues_[kUptimeRow], text);

    std::snprintf(
        text,
        sizeof(text),
        "%u KB free, min %u KB",
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
        static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024));
    lv_label_set_text(overviewValues_[kMemoryRow], text);

    std::snprintf(
        text,
        sizeof(text),
        "%u KB",
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
    lv_label_set_text(overviewValues_[kLargestBlockRow], text);

    const std::size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psramTotal == 0) {
        lv_label_set_text_static(overviewValues_[kPsramRow], "None");
    } else {
        std::snprintf(
            text,
            sizeof(text),
            "%u KB free of %u KB",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
            static_cast<unsigned>(psramTotal / 1024));
        lv_label_set_text(overviewValues_[kPsramRow], text);
    }

    const int batteryLevel = m5_platform::batteryLevelPercent();
    const int batteryMv = m5_platform::batteryVoltageMv();
    if (batteryLevel < 0) {
        lv_label_set_text_static(overviewValues_[kBatteryRow], "Unknown");
    } else {
        std::snprintf(
            text,
            sizeof(text),
            "%d%%, %d.%02d V%s",
            batteryLevel,
            batteryMv / 1000,
            (batteryMv % 1000) / 10,
            m5_platform::batteryCharging() ? ", charging" : "");
        lv_label_set_text(overviewValues_[kBatteryRow], text);
    }

    switch (client_.connectionState()) {
        case ossm::OssmClient::ConnectionState::Connected:
            std::snprintf(
                text,
                sizeof(text),
                "%s (%s)",
                ossmName_[0] != '\0' ? ossmName_ : "Connected",
                firmwareFamilyName(client_.firmwareFamily()));
            lv_label_set_text(overviewValues_[kOssmRow], text);
            break;
        case ossm::OssmClient::ConnectionState::Connecting:
            lv_label_set_text_static(overviewValues_[kOssmRow], "Connecting");
            break;
        case ossm::OssmClient::ConnectionState::Disconnected:
        case ossm::OssmClient::ConnectionState::Disconnecting:
            lv_label_set_text_static(overviewValues_[kOssmRow], "Not connected");
            break;
    }

    const int rssi = client_.rssi();
    if (rssi == 0) {
        lv_label_set_text_static(overviewValues_[kSignalRow], "-");
    } else {
        std::snprintf(text, sizeof(text), "%d dBm", rssi);
        lv_label_set_text(overviewValues_[kSignalRow], text);
    }

    if (NimBLEDevice::isInitialized()) {
        const std::string address = NimBLEDevice::getAddress().toString();
        lv_label_set_text(overviewValues_[kRemoteAddressRow], address.c_str());
    } else {
        lv_label_set_text_static(overviewValues_[kRemoteAddressRow], "-");
    }

    std::snprintf(
        text,
        sizeof(text),
        "%lu bytes free (min)",
        static_cast<unsigned long>(client_.workerStackMinimumFree()));
    lv_label_set_text(overviewValues_[kWorkerStackRow], text);

    const logging::LogCounts counts = logging::currentBootCounts();
    std::snprintf(
        text,
        sizeof(text),
        "%lu error%s, %lu warning%s",
        static_cast<unsigned long>(counts.errors),
        plural(counts.errors),
        static_cast<unsigned long>(counts.warnings),
        plural(counts.warnings));
    lv_label_set_text(overviewValues_[kLogRow], text);
}

void DiagnosticsScreen::renderLog() {
    std::uint32_t first = 0;
    std::uint32_t end = 0;
    logging::recordRange(first, end);
    renderedLogEnd_ = end;

    // Walk from the newest record backwards, counting display lines from the bottom, and keep
    // the ones inside the visible window.
    std::size_t produced = 0;
    auto emit = [&](const char* line) {
        if (produced >= logScrollLines_ && produced < logScrollLines_ + logRows_) {
            const std::size_t slot = logRows_ - 1 - (produced - logScrollLines_);
            std::snprintf(logLines_[slot], sizeof(logLines_[slot]), "%s", line);
        }
        ++produced;
    };

    bool sawCurrentBoot = false;
    bool sawPreviousBoot = false;
    bool currentSeparatorEmitted = false;
    logging::LogRecord record;
    char lines[kMaxLinesPerRecord][kWrapLineCapacity];
    for (std::uint32_t sequence = end; sequence != first;) {
        --sequence;
        if (!logging::readRecord(sequence, record)) {
            continue;
        }

        if (record.previousBoot) {
            if (sawCurrentBoot && !currentSeparatorEmitted) {
                emit(kCurrentBootSeparator);
                currentSeparatorEmitted = true;
            }
            sawPreviousBoot = true;
        } else {
            sawCurrentBoot = true;
        }

        const std::size_t lineCount = wrapRecord(record, logColumns_, lines);
        for (std::size_t index = lineCount; index-- > 0;) {
            emit(lines[index]);
        }
    }
    if (sawPreviousBoot) {
        emit(kPreviousBootSeparator);
    }

    logTotalLines_ = produced;
    const std::size_t maxScroll = produced > logRows_ ? produced - logRows_ : 0;
    if (logScrollLines_ > maxScroll) {
        logScrollLines_ = maxScroll;
        renderLog();
        return;
    }

    // With fewer lines than rows, the filled slots are at the bottom; show them from the top.
    const std::size_t visible = std::min(logRows_, produced - logScrollLines_);
    char* out = logText_;
    const char* const textEnd = logText_ + sizeof(logText_) - 1;
    for (std::size_t slot = logRows_ - visible; slot < logRows_; ++slot) {
        const std::size_t length = std::strlen(logLines_[slot]);
        if (out + length + 1 > textEnd) {
            break;
        }
        if (out != logText_) {
            *out++ = '\n';
        }
        std::memcpy(out, logLines_[slot], length);
        out += length;
    }
    *out = '\0';
    lv_label_set_text_static(logLabel_, logText_);
    // The buffer is reused, so make sure the label redraws even if its pointer is unchanged.
    lv_obj_invalidate(logLabel_);

    updateLogStatus(false);
}

void DiagnosticsScreen::scrollLog(std::int32_t linesBack) {
    const std::int64_t maxScroll =
        logTotalLines_ > logRows_ ? static_cast<std::int64_t>(logTotalLines_ - logRows_) : 0;
    std::int64_t next = static_cast<std::int64_t>(logScrollLines_) + linesBack;
    next = std::max<std::int64_t>(0, std::min(next, maxScroll));
    if (static_cast<std::size_t>(next) == logScrollLines_) {
        return;
    }

    logScrollLines_ = static_cast<std::size_t>(next);
    renderLog();
}

void DiagnosticsScreen::updateLogStatus(bool newEntriesBelow) {
    if (logTotalLines_ == 0) {
        lv_label_set_text_static(logStatusLabel_, "No log entries yet");
    } else if (logScrollLines_ == 0) {
        lv_label_set_text_static(logStatusLabel_, "Newest");
    } else {
        lv_label_set_text_fmt(
            logStatusLabel_,
            "%u lines back%s",
            static_cast<unsigned>(logScrollLines_),
            newEntriesBelow ? ", new entries below" : "");
    }

    if (logScrollLines_ == 0) {
        lv_obj_add_state(objects.diagnostics_action_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(objects.diagnostics_action_btn, LV_STATE_DISABLED);
    }
}

}  // namespace m5_redux
