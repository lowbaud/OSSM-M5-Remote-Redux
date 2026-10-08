#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "menu/MenuController.h"
#include "platform/RemoteInput.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

// Draws the menu as a panel that slides in from the left over the current screen, with its own
// button bar over the screen's. Everything lives on LVGL's top layer and is built once; a row is
// created the first time its item is shown and kept for later pages.
class MenuPanel {
  public:
    explicit MenuPanel(MenuController& controller);

    void begin();
    void open(MenuPage& root);
    void close();
    // Hides the panel but keeps the open pages, e.g. while the diagnostics screen is shown.
    void hide();
    // Shows hidden pages again after repopulating them.
    void show();
    // Repopulates the open pages after the device or settings changed.
    void refresh();
    MenuEvent update(const RemoteInputEvents& events);
    void setStopAvailable(bool available);
    void setMotionActive(bool active);

  private:
    struct RowView {
        MenuItem* item = nullptr;
        lv_obj_t* list = nullptr;
        lv_obj_t* row = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* value = nullptr;
        lv_obj_t* level = nullptr;
    };

    struct PageView {
        lv_obj_t* title = nullptr;
        lv_obj_t* list = nullptr;
        char titleText[32] = {};
    };

    // Settings, the quick actions and the most options a choice can list.
    static constexpr std::size_t kMaxRows = 64;

    MenuController& controller_;
    std::array<RowView, kMaxRows> rows_{};
    std::size_t rowCount_ = 0;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* scrim_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    PageView panelPage_;
    lv_obj_t* popupScrim_ = nullptr;
    lv_obj_t* popup_ = nullptr;
    PageView popupPage_;
    lv_obj_t* backLabel_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* selectButton_ = nullptr;
    lv_obj_t* selectLabel_ = nullptr;
    lv_obj_t* saveFailureLabel_ = nullptr;
    std::uint32_t saveFailureShownAtMs_ = 0;
    StopButtonFeedback stopButtonFeedback_;
    std::int32_t panelWidth_ = 0;
    std::size_t tappedRow_ = MenuPage::kNoItem;
    bool backTapped_ = false;
    bool selectTapped_ = false;
    bool closeTapped_ = false;
    bool visible_ = false;

    void buildPanel(std::int32_t height);
    void buildPopup();
    void buildButtonBar();
    void buildSaveFailureLabel();
    void buildPageView(PageView& view, lv_obj_t* parent, std::int32_t listHeight);
    void clearTaps();
    void slideIn();
    void slideOut();
    void renderAll();
    void render(const MenuChanges& changes);
    // The deepest page drawn in the panel; popups above it are drawn over it.
    std::size_t panelLevel() const;
    void showPage(PageView& view, std::size_t level);
    void updateHighlight(PageView& view, std::size_t level);
    void updateValues(std::size_t level);
    void updateButtonBar();
    void resizePopup(std::size_t rowCount);
    void showSaveFailure();
    void hideSaveFailure();
    void setTitle(PageView& view, const char* title);
    RowView* rowFor(MenuItem& item, lv_obj_t* list);
    RowView* findRow(const MenuItem& item);
    void createRow(RowView& view);
    void handleRowClicked(lv_obj_t* row);

    static void refreshRow(const RowView& view);
    static void setPanelX(void* panel, std::int32_t x);
    static void setScrimOpacity(void* scrim, std::int32_t opacity);
    static void handleSlideOutCompleted(lv_anim_t* animation);
    static void handleRowEvent(lv_event_t* event);
    static void handleBackEvent(lv_event_t* event);
    static void handleSelectEvent(lv_event_t* event);
    static void handleScrimEvent(lv_event_t* event);
};

}  // namespace m5_redux
