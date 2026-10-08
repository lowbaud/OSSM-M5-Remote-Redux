#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "menu/MenuController.h"
#include "platform/RemoteInput.h"
#include "ui/MenuItemView.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

// Draws the menu over the current screen, with its own button bar over the screen's. A tile
// page, like the quick actions, is a see-through panel that slides in from the left above the
// button bar. A list page, like the settings, fills the screen like the old settings screen.
// Everything lives on LVGL's top layer and is built once; an item's widgets are created the
// first time it is shown and kept.
class MenuPanel {
  public:
    explicit MenuPanel(MenuController& controller);

    void begin();
    void open(MenuPage& root);
    void close();
    // Hides the menu but keeps the open pages, e.g. while the diagnostics screen is shown.
    void hide();
    // Shows hidden pages again after repopulating them.
    void show();
    // Repopulates the open pages after the device or settings changed.
    void refresh();
    MenuEvent update(const RemoteInputEvents& events);
    void setStopAvailable(bool available);
    void setMotionActive(bool active);

  private:
    struct PageView {
        // Popups have a title; the panel and the full-screen page do not.
        lv_obj_t* title = nullptr;
        // Either a list or a tile grid.
        lv_obj_t* container = nullptr;
        bool tiles = false;
        char titleText[32] = {};
    };

    // Settings, the quick actions and the most options a choice can list.
    static constexpr std::size_t kMaxViews = 64;

    MenuController& controller_;
    std::array<MenuItemView, kMaxViews> views_{};
    std::size_t viewCount_ = 0;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    PageView panelPage_;
    lv_obj_t* screen_ = nullptr;
    PageView screenPage_;
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
    std::size_t tappedRow_ = MenuPage::kNoItem;
    bool backTapped_ = false;
    bool selectTapped_ = false;
    bool closeTapped_ = false;
    bool visible_ = false;
    // The panel is in place or sliding in.
    bool panelIn_ = false;

    void buildPanel(std::int32_t buttonBarTop);
    void buildScreen(std::int32_t buttonBarTop);
    void buildPopup();
    void buildButtonBar();
    void buildSaveFailureLabel();
    void clearTaps();
    void slideIn();
    void slideOut();
    void movePanelIn();
    void hideNow();
    void renderAll();
    void render(const MenuChanges& changes);
    // The deepest page that is not a popup; popups above it are drawn over it.
    std::size_t baseLevel() const;
    bool drawsTiles(std::size_t level) const;
    PageView& pageView(std::size_t level);
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

    static void setPanelX(void* panel, std::int32_t x);
    static void handleSlideOutCompleted(lv_anim_t* animation);
    static void handleViewEvent(lv_event_t* event);
    static void handleBackEvent(lv_event_t* event);
    static void handleSelectEvent(lv_event_t* event);
    static void handleCloseEvent(lv_event_t* event);
};

}  // namespace m5_redux
