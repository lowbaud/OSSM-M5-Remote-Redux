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

// The menu on the EEZ menu screen: the pages as a list with action tiles at the top, option
// popups, and the message when saving fails. EEZ holds the screen, the popup and the button bar;
// the rows and tiles are created here the first time an item is shown, and kept.
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
    // Shows a quick action that started or ended while the menu is open on its tile.
    void refreshTiles();
    // Back and Select taps, also the tap beside an open popup; handled on the next update.
    void requestBack();
    void requestSelect();

  private:
    struct PageView {
        // Only popups have a title.
        lv_obj_t* title = nullptr;
        lv_obj_t* container = nullptr;
        // Only the screen's list has tiles; the row is its first child.
        lv_obj_t* tileRow = nullptr;
        // The page shown last; another page starts scrolled to the top.
        const MenuPage* shownPage = nullptr;
        char titleText[32] = {};
    };

    // Rows and tiles of the pages; an item on two pages shares its view. Raise it when the pages
    // outgrow it.
    static constexpr std::size_t kMaxPageViews = 24;
    // The options popup reuses its rows for every choice, so options never need more views than
    // a page holds rows.
    static constexpr std::size_t kMaxViews = kMaxPageViews + MenuPage::kMaxItems;

    MenuController controller_;
    std::array<MenuItemView, kMaxViews> views_{};
    std::size_t viewCount_ = 0;
    std::size_t pageViewCount_ = 0;
    PageView pageView_;
    const char* rootBackLabel_ = "Back";
    PageView popupView_;
    std::uint32_t saveFailureShownAtMs_ = 0;
    StopButtonFeedback stopButtonFeedback_;
    std::size_t tappedRow_ = MenuPage::kNoItem;
    bool backTapped_ = false;
    bool selectTapped_ = false;

    void load();
    void closePages();
    void clearInput();
    void renderAll();
    void render(const MenuChanges& changes);
    // The page in the list; an open popup is drawn over it.
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
};

}  // namespace m5_redux
