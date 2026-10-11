#include "MenuScreen.h"

#include <Arduino.h>

#include <algorithm>
#include <cctype>

#include "diagnostics/Log.h"
#include "ui/ListRowStyle.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr const char* kTag = "menu";
// The popup grows with its rows up to this many, then scrolls.
constexpr std::size_t kMaxVisiblePopupRows = 4;
constexpr std::uint32_t kSaveFailureDurationMs = 2000;

// Setting the same static text again would still redraw the label.
void setStaticText(lv_obj_t* label, const char* text) {
    if (lv_label_get_text(label) != text) {
        lv_label_set_text_static(label, text);
    }
}

void setHidden(lv_obj_t* object, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

// The EEZ containers that hold the rows; the rows stack and scroll vertically.
void setUpList(lv_obj_t* list) {
    addSharedListStyles(list);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
}

}  // namespace

void MenuScreen::begin() {
    setUpList(objects.menu_list);
    pageView_.container = objects.menu_list;
    // The tiles are the first child of the list, so the EEZ list has no children of its own.
    pageView_.tileRow = createMenuTileRow(objects.menu_list);

    setUpList(objects.menu_popup_list);
    popupView_.container = objects.menu_popup_list;
    popupView_.title = objects.menu_popup_title;
    lv_label_set_text_static(popupView_.title, popupView_.titleText);

    stopButtonFeedback_.begin(objects.menu_stop_btn);
    // Shown only while a popup is open or after a failed save, whatever the EEZ design shows.
    lv_obj_add_flag(objects.menu_popup_scrim, LV_OBJ_FLAG_HIDDEN);
    hideSaveFailure();
    setStopAvailable(false);
}

void MenuScreen::open(MenuPage& root, const char* rootBackLabel) {
    rootBackLabel_ = rootBackLabel;
    controller_.open(root);
    clearInput();
    hideSaveFailure();
    renderAll();
    load();
}

void MenuScreen::resume() {
    if (controller_.isOpen()) {
        controller_.refresh();
        clearInput();
        renderAll();
    }
    load();
}

void MenuScreen::close() {
    closePages();
}

bool MenuScreen::isOpen() const {
    return controller_.isOpen();
}

MenuEvent MenuScreen::update(const RemoteInputEvents& events) {
    if (!controller_.isOpen()) {
        clearInput();
        return {};
    }

    if (!lv_obj_has_flag(objects.menu_save_failure_lbl, LV_OBJ_FLAG_HIDDEN) &&
        millis() - saveFailureShownAtMs_ >= kSaveFailureDurationMs) {
        hideSaveFailure();
    }

    MenuInput input;
    input.steps = events.menuSteps();
    // Raw knob directions, so the stroke direction setting has no effect in the menu.
    input.adjustSteps = events.encoderSteps[1] + events.encoderSteps[2];
    input.back = events.leftClick || backTapped_;
    input.select = events.rightClick || selectTapped_;
    input.tappedRow = tappedRow_;
    clearInput();

    const MenuEvent event = controller_.update(input);
    if (event.type == MenuEvent::Type::Closed) {
        closePages();
        return event;
    }

    render(controller_.takeChanges());
    return event;
}

void MenuScreen::setStopAvailable(bool available) {
    if (available) {
        lv_obj_remove_flag(objects.menu_stop_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(objects.menu_stop_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuScreen::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void MenuScreen::refreshTiles() {
    if (!controller_.isOpen()) {
        return;
    }

    const MenuPage& page = controller_.page(baseLevel());
    for (std::size_t index = 0; index < page.tileCount(); ++index) {
        const MenuItemView* view = findView(page.item(index), true);
        if (view != nullptr) {
            refreshMenuItemView(*view);
        }
    }
    // A highlighted tile's select label can change with it, e.g. Ramp to End.
    updateButtonBar();
}

void MenuScreen::requestBack() {
    backTapped_ = true;
}

void MenuScreen::requestSelect() {
    selectTapped_ = true;
}

void MenuScreen::load() {
    if (lv_screen_active() != objects.menu) {
        loadScreen(SCREEN_ID_MENU);
    }
}

void MenuScreen::closePages() {
    controller_.close();
    controller_.takeChanges();
    clearInput();
    hideSaveFailure();
    lv_obj_add_flag(objects.menu_popup_scrim, LV_OBJ_FLAG_HIDDEN);
    // Opening the menu again starts at the top.
    pageView_.shownPage = nullptr;
    popupView_.shownPage = nullptr;
}

void MenuScreen::clearInput() {
    tappedRow_ = MenuPage::kNoItem;
    backTapped_ = false;
    selectTapped_ = false;
}

void MenuScreen::renderAll() {
    MenuChanges changes = controller_.takeChanges();
    changes.pages = true;
    render(changes);
}

void MenuScreen::render(const MenuChanges& changes) {
    if (!controller_.isOpen()) {
        return;
    }
    // Nothing to redraw; touching the widgets anyway would still make LVGL redraw them.
    if (!changes.pages && !changes.highlight && !changes.values && !changes.saveFailed) {
        return;
    }

    const std::size_t top = controller_.depth() - 1;
    const std::size_t base = baseLevel();
    if (changes.pages) {
        showPage(pageView_, base);
        if (top != base) {
            showPage(popupView_, top);
            resizePopup(controller_.page(top).count());
            lv_obj_remove_flag(objects.menu_popup_scrim, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.menu_popup_scrim, LV_OBJ_FLAG_HIDDEN);
            // One options page serves every choice, so the next popup has to start at the top
            // even though its page is the same.
            popupView_.shownPage = nullptr;
        }
    } else {
        if (changes.highlight) {
            updateHighlight(top);
        }
        if (changes.values) {
            updateValues(top);
            if (top != base) {
                updateValues(base);
            }
        }
    }

    updateButtonBar();
    if (changes.saveFailed) {
        showSaveFailure();
    }
}

std::size_t MenuScreen::baseLevel() const {
    return controller_.depth() - (controller_.popupOpen() ? 2 : 1);
}

MenuScreen::PageView& MenuScreen::viewOf(std::size_t level) {
    return level > baseLevel() ? popupView_ : pageView_;
}

void MenuScreen::showPage(PageView& view, std::size_t level) {
    MenuPage& page = controller_.page(level);
    if (view.title != nullptr) {
        setTitle(view, page.title());
    }

    for (std::size_t index = 0; index < viewCount_; ++index) {
        const lv_obj_t* container = views_[index].container;
        if (container == view.container || (view.tileRow != nullptr && container == view.tileRow)) {
            lv_obj_add_flag(views_[index].root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    if (view.tileRow != nullptr) {
        setHidden(view.tileRow, tiles == 0);
    }
    // Rows follow the tile row, which stays the first child of the list.
    const std::size_t firstRow = view.tileRow != nullptr ? 1 : 0;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const bool tile = index < tiles;
        MenuItemView* itemView =
            viewFor(page.item(index), tile ? view.tileRow : view.container, tile);
        if (itemView == nullptr) {
            break;
        }
        const std::size_t position = tile ? index : index - tiles + firstRow;
        lv_obj_move_to_index(itemView->root, static_cast<std::int32_t>(position));
        lv_obj_remove_flag(itemView->root, LV_OBJ_FLAG_HIDDEN);
        refreshMenuItemView(*itemView);
    }

    // Scrolling the highlight into view only scrolls as far as needed, so a page left scrolled
    // down, e.g. Settings, would otherwise keep the tiles of the page before it out of view.
    // The same page again, e.g. under a closing popup, stays where it was.
    if (view.shownPage != &page) {
        view.shownPage = &page;
        lv_obj_scroll_to_y(view.container, 0, LV_ANIM_OFF);
    }
    updateHighlight(level);
}

void MenuScreen::updateHighlight(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    const std::size_t highlight = controller_.highlight(level);
    lv_obj_t* highlighted = nullptr;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), index < tiles);
        if (itemView == nullptr) {
            continue;
        }
        const bool selected = index == highlight;
        setMenuItemViewSelected(*itemView, selected);
        if (selected) {
            // Tiles scroll with their row, which the list holds.
            highlighted = itemView->tile ? view.tileRow : itemView->root;
        }
    }

    if (highlighted != nullptr) {
        lv_obj_scroll_to_view(highlighted, LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(view.container, 0, LV_ANIM_OFF);
    }
}

void MenuScreen::updateValues(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), index < tiles);
        if (itemView != nullptr) {
            refreshMenuItemView(*itemView);
        }
    }
}

void MenuScreen::updateButtonBar() {
    setStaticText(objects.menu_back_lbl, controller_.depth() > 1 ? "Back" : rootBackLabel_);

    const MenuItem* item = controller_.highlightedItem();
    setStaticText(objects.menu_select_lbl, item != nullptr ? item->selectLabel() : "Select");
    if (item != nullptr) {
        lv_obj_remove_state(objects.menu_select_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(objects.menu_select_btn, LV_STATE_DISABLED);
    }
}

void MenuScreen::resizePopup(std::size_t rowCount) {
    const std::size_t visibleRows = std::min(rowCount, kMaxVisiblePopupRows);
    const std::int32_t listHeight = static_cast<std::int32_t>(visibleRows) * listRowHeight();
    lv_obj_set_height(popupView_.container, listHeight);
    // The list ends the popup, wherever the EEZ design places it below the title.
    const std::int32_t listTop = lv_obj_get_style_y(popupView_.container, LV_PART_MAIN);
    const std::int32_t border = lv_obj_get_style_border_width(objects.menu_popup, LV_PART_MAIN);
    lv_obj_set_height(objects.menu_popup, listTop + listHeight + 2 * border);
}

void MenuScreen::showSaveFailure() {
    saveFailureShownAtMs_ = millis();
    lv_obj_remove_flag(objects.menu_save_failure_lbl, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::hideSaveFailure() {
    lv_obj_add_flag(objects.menu_save_failure_lbl, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::setTitle(PageView& view, const char* title) {
    std::size_t length = 0;
    for (; title[length] != '\0' && length < sizeof(view.titleText) - 1; ++length) {
        view.titleText[length] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(title[length])));
    }
    view.titleText[length] = '\0';
    lv_label_set_text_static(view.title, view.titleText);
}

MenuItemView* MenuScreen::viewFor(MenuItem& item, lv_obj_t* container, bool tile) {
    MenuItemView* existing = findView(item, tile);
    if (existing != nullptr) {
        return existing;
    }

    // Options have room of their own, so a long list of them never crowds out the other rows.
    const bool option = item.kind() == MenuItemKind::Option;
    if (viewCount_ >= views_.size() || (!option && pageViewCount_ >= kMaxPageViews)) {
        LOGE(kTag, "No room for menu item %s; raise MenuScreen::kMaxPageViews", item.name());
        return nullptr;
    }
    if (!option) {
        ++pageViewCount_;
    }

    MenuItemView& view = views_[viewCount_++];
    view.item = &item;
    if (tile) {
        createMenuTile(view, container);
    } else {
        createMenuRow(view, container);
    }
    lv_obj_add_event_cb(view.root, handleViewEvent, LV_EVENT_CLICKED, this);
    return &view;
}

MenuItemView* MenuScreen::findView(const MenuItem& item, bool tile) {
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].item == &item && views_[index].tile == tile) {
            return &views_[index];
        }
    }
    return nullptr;
}

void MenuScreen::handleViewClicked(lv_obj_t* root) {
    if (!controller_.isOpen()) {
        return;
    }

    const MenuPage& page = controller_.page(controller_.depth() - 1);
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].root == root) {
            tappedRow_ = page.indexOf(*views_[index].item);
            return;
        }
    }
}

void MenuScreen::handleViewEvent(lv_event_t* event) {
    auto* menu = static_cast<MenuScreen*>(lv_event_get_user_data(event));
    menu->handleViewClicked(static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
}

}  // namespace m5_redux
