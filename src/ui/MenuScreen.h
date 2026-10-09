#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "menu/MenuController.h"
#include "menu/MenuPage.h"
#include "platform/RemoteInput.h"
#include "ui/MenuItemView.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

// The menu on a screen of its own: an optional status line, the pages as a list with action
// tiles at the top, option popups, a button bar like the other screens, and the message when
// saving fails. App shows it like any other screen; the next screen App loads replaces it.
// Everything is built once; an item's widgets are created the first time it is shown and kept.
class MenuScreen {
  public:
    MenuScreen() = default;
    MenuScreen(const MenuScreen&) = delete;
    MenuScreen& operator=(const MenuScreen&) = delete;

    void begin();
    // Opens the root page and shows the screen. The back button reads rootBackLabel on the root
    // page, e.g. "Close" or "Back".
    void open(MenuPage& root, const char* rootBackLabel);
    // Shows the open pages again, e.g. back from the diagnostics screen, on the same row.
    void resume();
    void close();
    bool isOpen() const;
    // Feeds the knobs, buttons and taps to the open pages and redraws what changed.
    MenuEvent update(const RemoteInputEvents& events);
    void setStopAvailable(bool available);
    void setMotionActive(bool active);
    // The pattern, speed and sensation of the device under control, above the list.
    void showStatus(const char* pattern, int speed, int sensation);
    void hideStatus();

  private:
    struct PageView {
        // Only popups have a title.
        lv_obj_t* title = nullptr;
        lv_obj_t* container = nullptr;
        // Only the screen's list has tiles; the row is its first child.
        lv_obj_t* tileRow = nullptr;
        char titleText[32] = {};
    };

    // The settings and the most options a choice can list.
    static constexpr std::size_t kMaxViews = 48;

    MenuController controller_;
    std::array<MenuItemView, kMaxViews> views_{};
    std::size_t viewCount_ = 0;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* list_ = nullptr;
    std::int32_t listBottom_ = 0;
    PageView pageView_;
    const char* rootBackLabel_ = "Back";
    lv_obj_t* statusBar_ = nullptr;
    lv_obj_t* statusDot_ = nullptr;
    lv_obj_t* statusPattern_ = nullptr;
    lv_obj_t* statusSpeed_ = nullptr;
    lv_obj_t* statusSensation_ = nullptr;
    char patternText_[32] = {};
    char speedText_[12] = {};
    char sensationText_[12] = {};
    lv_obj_t* popupScrim_ = nullptr;
    lv_obj_t* popup_ = nullptr;
    PageView popupView_;
    lv_obj_t* backLabel_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* selectButton_ = nullptr;
    lv_obj_t* selectLabel_ = nullptr;
    lv_obj_t* saveFailureLabel_ = nullptr;
    std::uint32_t saveFailureShownAtMs_ = 0;
    StopButtonFeedback stopButtonFeedback_;
    std::size_t tappedRow_ = MenuPage::kNoItem;
    bool backTapped_ = false;
    bool selectTapped_ = false;

    void buildStatusBar();
    void buildPopup();
    void buildButtonBar();
    void buildSaveFailureLabel();
    void load();
    void closePages();
    void clearInput();
    void renderAll();
    void render(const MenuChanges& changes);
    // The deepest page that is not a popup; popups above it are drawn over it.
    std::size_t baseLevel() const;
    PageView& viewOf(std::size_t level);
    void showPage(PageView& view, std::size_t level);
    void updateHighlight(std::size_t level);
    void updateValues(std::size_t level);
    void updateButtonBar();
    void resizePopup(std::size_t rowCount);
    void showSaveFailure();
    void hideSaveFailure();
    void setTitle(PageView& view, const char* title);
    MenuItemView* viewFor(MenuItem& item, lv_obj_t* container, bool tile);
    MenuItemView* findView(const MenuItem& item, bool tile);
    void handleViewClicked(lv_obj_t* root);

    static void handleViewEvent(lv_event_t* event);
    static void handleBackEvent(lv_event_t* event);
    static void handleSelectEvent(lv_event_t* event);
};

}  // namespace m5_redux
