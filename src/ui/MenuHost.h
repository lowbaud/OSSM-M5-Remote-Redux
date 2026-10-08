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

// Shows menu pages: the pages themselves in a container of the subclass, option popups, a button
// bar over the one of the screens, and the message when saving fails. Owns the page stack.
// Everything is built once; an item's widgets are created the first time it is shown and kept.
class MenuHost {
  public:
    MenuHost(const MenuHost&) = delete;
    MenuHost& operator=(const MenuHost&) = delete;
    virtual ~MenuHost() = default;

    bool isOpen() const;
    // Feeds the knobs, buttons and taps to the open pages and redraws what changed.
    MenuEvent update(const RemoteInputEvents& events);
    void setStopAvailable(bool available);
    void setMotionActive(bool active);

  protected:
    struct PageLayout {
        // Where pages are drawn; popups have their own list.
        lv_obj_t* container = nullptr;
        bool tiles = false;
        std::int32_t tileWidth = 0;
        // Label of the back button on the first page.
        const char* rootBackLabel = "Back";
    };

    MenuHost() = default;

    // Builds the popup, button bar and save message in parent, above what is already there.
    void buildChrome(lv_obj_t* parent, const PageLayout& layout);
    void openPages(MenuPage& root);
    void closePages();
    // Repopulates the open pages and redraws them, e.g. when shown again.
    void refreshPages();
    // Closes the pages on the next update, e.g. after a tap beside the panel.
    void requestClose();
    // Called when Back on the first page or requestClose() closed the pages.
    virtual void pagesClosed();

  private:
    struct PageView {
        // Only popups have a title.
        lv_obj_t* title = nullptr;
        lv_obj_t* container = nullptr;
        bool tiles = false;
        char titleText[32] = {};
    };

    // The settings and the most options a choice can list.
    static constexpr std::size_t kMaxViews = 48;

    MenuController controller_;
    std::array<MenuItemView, kMaxViews> views_{};
    std::size_t viewCount_ = 0;
    PageView pageView_;
    std::int32_t tileWidth_ = 0;
    const char* rootBackLabel_ = "Back";
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
    bool closeRequested_ = false;

    void buildPopup(lv_obj_t* parent);
    void buildButtonBar(lv_obj_t* parent);
    void buildSaveFailureLabel(lv_obj_t* parent);
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
