#include <unity.h>

#include <utility>
#include <vector>

#include "menu/MenuController.h"
#include "menu/MenuItems.h"
#include "menu/MenuPage.h"

using namespace m5_redux;

namespace {

constexpr MenuCommand kDisconnectCommand = 1;
constexpr MenuCommand kDiagnosticsCommand = 2;

class FakeChoice : public MenuChoiceItem {
  public:
    FakeChoice(const char* name, std::vector<const char*> optionNames, std::size_t storedIndex)
        : MenuChoiceItem(name, name), names(std::move(optionNames)), stored(storedIndex),
          applied(storedIndex) {}

    std::size_t optionCount() const override {
        return names.size();
    }

    const char* optionName(std::size_t index) const override {
        return names[index];
    }

    std::size_t storedOption() const override {
        return stored;
    }

    bool commitOption(std::size_t index) override {
        commits.push_back(index);
        if (!commitSucceeds) {
            return false;
        }
        stored = index;
        applied = index;
        return true;
    }

    void previewOption(std::size_t index) override {
        previews.push_back(index);
        applied = index;
    }

    void cancelPreview() override {
        ++cancels;
        applied = stored;
    }

    std::vector<const char*> names;
    std::size_t stored;
    std::size_t applied;
    bool commitSucceeds = true;
    std::vector<std::size_t> commits;
    std::vector<std::size_t> previews;
    int cancels = 0;
};

class FakeSource : public MenuSource {
  public:
    void addItems(MenuPage& page) override {
        for (MenuItem* tile : tiles) {
            page.addTile(*tile);
        }
        for (MenuItem* item : items) {
            page.add(*item);
        }
    }

    std::vector<MenuItem*> tiles;
    std::vector<MenuItem*> items;
};

MenuInput stepInput(std::int32_t steps) {
    MenuInput input;
    input.steps = steps;
    return input;
}

MenuInput adjustInput(std::int32_t steps) {
    MenuInput input;
    input.adjustSteps = steps;
    return input;
}

MenuInput selectInput() {
    MenuInput input;
    input.select = true;
    return input;
}

MenuInput backInput() {
    MenuInput input;
    input.back = true;
    return input;
}

std::vector<const char*> fourOptions() {
    return {"Low", "Medium", "High", "Maximum"};
}

void test_open_starts_on_the_first_row_that_is_not_destructive() {
    MenuCommandItem disconnect(
        "disconnect", "Disconnect", kDisconnectCommand, MenuCommandItem::Style::Destructive);
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&disconnect, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;

    menu.open(page);

    TEST_ASSERT_TRUE(menu.isOpen());
    TEST_ASSERT_EQUAL_UINT(1, menu.depth());
    TEST_ASSERT_EQUAL_UINT(1, menu.highlight(0));
    TEST_ASSERT_TRUE(menu.highlightedItem() == &diagnostics);
    TEST_ASSERT_TRUE(menu.takeChanges().pages);
}

void test_highlight_stops_at_the_first_and_last_row() {
    MenuCommandItem first("first", "First", 1);
    MenuCommandItem second("second", "Second", 2);
    MenuCommandItem third("third", "Third", 3);
    FakeSource source;
    source.items = {&first, &second, &third};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.takeChanges();

    menu.update(stepInput(10));
    TEST_ASSERT_EQUAL_UINT(2, menu.highlight(0));
    TEST_ASSERT_TRUE(menu.takeChanges().highlight);

    menu.update(stepInput(-10));
    TEST_ASSERT_EQUAL_UINT(0, menu.highlight(0));
    TEST_ASSERT_TRUE(menu.takeChanges().highlight);

    menu.update(stepInput(-1));
    TEST_ASSERT_FALSE(menu.takeChanges().highlight);
}

void test_back_on_the_root_page_closes_the_menu() {
    MenuCommandItem item("item", "Item", 1);
    FakeSource source;
    source.items = {&item};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);

    const MenuEvent event = menu.update(backInput());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuEvent::Type::Closed), static_cast<int>(event.type));
    TEST_ASSERT_FALSE(menu.isOpen());
    TEST_ASSERT_NULL(menu.highlightedItem());
}

void test_command_rows_hand_their_command_to_the_host() {
    MenuCommandItem diagnostics(
        "diagnostics", "Diagnostics", kDiagnosticsCommand, MenuCommandItem::Style::OpensScreen);
    FakeSource source;
    source.items = {&diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);

    const MenuEvent event = menu.update(selectInput());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuEvent::Type::Command), static_cast<int>(event.type));
    TEST_ASSERT_EQUAL_UINT(kDiagnosticsCommand, event.command);
    TEST_ASSERT_EQUAL_STRING("Open", diagnostics.selectLabel());
    TEST_ASSERT_TRUE(diagnostics.showsChevron());
    TEST_ASSERT_FALSE(diagnostics.destructive());
    TEST_ASSERT_TRUE(menu.isOpen());
}

void test_tapping_a_row_highlights_and_selects_it() {
    MenuCommandItem disconnect(
        "disconnect", "Disconnect", kDisconnectCommand, MenuCommandItem::Style::Destructive);
    MenuCommandItem diagnostics(
        "diagnostics", "Diagnostics", kDiagnosticsCommand, MenuCommandItem::Style::OpensScreen);
    FakeSource source;
    source.items = {&disconnect, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    TEST_ASSERT_EQUAL_STRING("Select", disconnect.selectLabel());
    TEST_ASSERT_FALSE(disconnect.showsChevron());

    MenuInput input;
    input.tappedRow = 0;
    const MenuEvent event = menu.update(input);

    TEST_ASSERT_EQUAL_UINT(0, menu.highlight(0));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuEvent::Type::Command), static_cast<int>(event.type));
    TEST_ASSERT_EQUAL_UINT(kDisconnectCommand, event.command);

    input.tappedRow = 2;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MenuEvent::Type::None), static_cast<int>(menu.update(input).type));
    TEST_ASSERT_EQUAL_UINT(0, menu.highlight(0));
}

void test_link_opens_a_page_and_back_returns_to_the_same_row() {
    MenuCommandItem brightness("brightness", "Brightness", 1);
    MenuCommandItem dim("dim", "Dim", 2);
    FakeSource settingsSource;
    settingsSource.items = {&brightness, &dim};
    MenuPage settings;
    settings.addSource(settingsSource);

    MenuCommandItem pause("pause", "Pause", 3);
    MenuLinkItem settingsLink("settings", "Settings", settings);
    FakeSource quickSource;
    quickSource.items = {&pause, &settingsLink};
    MenuPage quickActions;
    quickActions.addSource(quickSource);

    MenuController menu;
    menu.open(quickActions);
    menu.update(stepInput(1));
    menu.takeChanges();
    TEST_ASSERT_TRUE(settingsLink.showsChevron());
    TEST_ASSERT_EQUAL_STRING("Open", settingsLink.selectLabel());

    menu.update(selectInput());
    TEST_ASSERT_EQUAL_UINT(2, menu.depth());
    TEST_ASSERT_FALSE(menu.popupOpen());
    TEST_ASSERT_TRUE(&menu.page(1) == &settings);
    TEST_ASSERT_EQUAL_UINT(0, menu.highlight(1));
    TEST_ASSERT_TRUE(menu.takeChanges().pages);

    menu.update(stepInput(1));
    menu.update(backInput());
    TEST_ASSERT_EQUAL_UINT(1, menu.depth());
    TEST_ASSERT_TRUE(menu.highlightedItem() == &settingsLink);
    TEST_ASSERT_TRUE(menu.takeChanges().pages);
}

void test_pages_open_no_deeper_than_the_maximum_depth() {
    static_assert(MenuController::kMaxDepth == 4, "Extend the page chain");
    MenuPage pages[MenuController::kMaxDepth + 1];
    MenuLinkItem links[] = {
        MenuLinkItem("next", "Next", pages[1]),
        MenuLinkItem("next", "Next", pages[2]),
        MenuLinkItem("next", "Next", pages[3]),
        MenuLinkItem("next", "Next", pages[4])};
    FakeSource sources[MenuController::kMaxDepth];
    for (std::size_t index = 0; index < MenuController::kMaxDepth; ++index) {
        sources[index].items = {&links[index]};
        pages[index].addSource(sources[index]);
    }
    MenuController menu;
    menu.open(pages[0]);

    // One select more than the stack can hold.
    for (std::size_t index = 0; index < MenuController::kMaxDepth; ++index) {
        menu.update(selectInput());
    }

    TEST_ASSERT_EQUAL_UINT(MenuController::kMaxDepth, menu.depth());
    TEST_ASSERT_TRUE(&menu.page(MenuController::kMaxDepth - 1) == &pages[3]);
}

void test_choice_popup_previews_options_and_back_cancels_the_preview() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    FakeSource source;
    source.items = {&brightness};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);

    menu.update(selectInput());
    TEST_ASSERT_EQUAL_UINT(2, menu.depth());
    TEST_ASSERT_TRUE(menu.popupOpen());
    TEST_ASSERT_EQUAL_STRING("Brightness", menu.page(1).title());
    TEST_ASSERT_EQUAL_UINT(4, menu.page(1).count());
    TEST_ASSERT_EQUAL_UINT(1, menu.highlight(1));
    TEST_ASSERT_EQUAL_STRING("Medium", menu.highlightedItem()->name());

    menu.update(stepInput(1));
    TEST_ASSERT_EQUAL_UINT(2, brightness.applied);
    TEST_ASSERT_FALSE(static_cast<MenuOptionItem*>(menu.highlightedItem())->stored());

    menu.update(backInput());
    TEST_ASSERT_EQUAL_UINT(1, menu.depth());
    TEST_ASSERT_EQUAL_INT(1, brightness.cancels);
    TEST_ASSERT_EQUAL_UINT(1, brightness.applied);
    TEST_ASSERT_TRUE(brightness.commits.empty());
}

void test_choice_popup_select_saves_the_option_and_closes() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    FakeSource source;
    source.items = {&brightness};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(selectInput());
    menu.update(stepInput(2));
    menu.takeChanges();

    menu.update(selectInput());

    TEST_ASSERT_EQUAL_UINT(1, menu.depth());
    TEST_ASSERT_EQUAL_UINT(1, brightness.commits.size());
    TEST_ASSERT_EQUAL_UINT(3, brightness.stored);
    TEST_ASSERT_EQUAL_INT(0, brightness.cancels);
    TEST_ASSERT_EQUAL_STRING("Maximum", brightness.valueText());
    const MenuChanges changes = menu.takeChanges();
    TEST_ASSERT_TRUE(changes.pages);
    TEST_ASSERT_TRUE(changes.values);
    TEST_ASSERT_FALSE(changes.saveFailed);
}

void test_failed_popup_save_stays_open_on_the_stored_option() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    brightness.commitSucceeds = false;
    FakeSource source;
    source.items = {&brightness};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(selectInput());
    menu.update(stepInput(1));
    menu.takeChanges();

    menu.update(selectInput());

    TEST_ASSERT_EQUAL_UINT(2, menu.depth());
    TEST_ASSERT_EQUAL_UINT(1, menu.highlight(1));
    TEST_ASSERT_EQUAL_UINT(1, brightness.stored);
    TEST_ASSERT_EQUAL_UINT(1, brightness.applied);
    TEST_ASSERT_TRUE(menu.takeChanges().saveFailed);
}

void test_popup_without_a_matching_option_starts_without_a_highlight() {
    FakeChoice pattern("Default pattern", {"Auto", "Simple", "Teasing"}, MenuChoiceItem::kNoOption);
    FakeSource source;
    source.items = {&pattern};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    TEST_ASSERT_EQUAL_STRING("", pattern.valueText());

    menu.update(selectInput());
    TEST_ASSERT_EQUAL_UINT(2, menu.depth());
    TEST_ASSERT_TRUE(menu.highlight(1) == MenuController::kNoItem);

    menu.update(selectInput());
    TEST_ASSERT_TRUE(pattern.commits.empty());

    menu.update(stepInput(1));
    TEST_ASSERT_EQUAL_UINT(0, menu.highlight(1));
    TEST_ASSERT_EQUAL_UINT(1, pattern.previews.size());
    TEST_ASSERT_EQUAL_UINT(0, pattern.previews[0]);
}

void test_inline_adjustment_previews_and_saves_when_the_highlight_leaves() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&brightness, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.takeChanges();

    menu.update(adjustInput(1));
    TEST_ASSERT_TRUE(menu.takeChanges().values);
    TEST_ASSERT_EQUAL_UINT(2, brightness.applied);
    TEST_ASSERT_EQUAL_STRING("High", brightness.valueText());
    TEST_ASSERT_TRUE(brightness.commits.empty());

    menu.update(stepInput(1));
    TEST_ASSERT_EQUAL_UINT(1, brightness.commits.size());
    TEST_ASSERT_EQUAL_UINT(2, brightness.stored);
}

void test_inline_adjustment_back_to_the_stored_option_saves_nothing() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&brightness, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);

    menu.update(adjustInput(1));
    menu.update(adjustInput(-1));
    menu.update(stepInput(1));

    TEST_ASSERT_TRUE(brightness.commits.empty());
    TEST_ASSERT_EQUAL_UINT(1, brightness.applied);
}

void test_failed_inline_save_cancels_the_preview_and_reports_it() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    brightness.commitSucceeds = false;
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&brightness, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(adjustInput(2));
    menu.takeChanges();

    menu.update(stepInput(1));

    TEST_ASSERT_TRUE(menu.takeChanges().saveFailed);
    TEST_ASSERT_EQUAL_INT(1, brightness.cancels);
    TEST_ASSERT_EQUAL_UINT(1, brightness.applied);
    TEST_ASSERT_EQUAL_STRING("Medium", brightness.valueText());
}

void test_closing_the_menu_saves_a_pending_inline_adjustment() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    FakeSource source;
    source.items = {&brightness};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(adjustInput(-1));

    menu.close();

    TEST_ASSERT_FALSE(menu.isOpen());
    TEST_ASSERT_EQUAL_UINT(1, brightness.commits.size());
    TEST_ASSERT_EQUAL_UINT(0, brightness.stored);
}

void test_select_saves_a_pending_inline_adjustment_before_opening_the_options() {
    FakeChoice brightness("Brightness", fourOptions(), 1);
    FakeSource source;
    source.items = {&brightness};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(adjustInput(1));

    menu.update(selectInput());

    TEST_ASSERT_EQUAL_UINT(2, menu.depth());
    TEST_ASSERT_EQUAL_UINT(2, brightness.stored);
    TEST_ASSERT_EQUAL_UINT(2, menu.highlight(1));
}

void test_refresh_keeps_the_highlight_on_the_same_row() {
    MenuCommandItem disconnect(
        "disconnect", "Disconnect", kDisconnectCommand, MenuCommandItem::Style::Destructive);
    MenuCommandItem pattern("pattern", "Default pattern", 1);
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&disconnect, &pattern, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(stepInput(1));
    TEST_ASSERT_TRUE(menu.highlightedItem() == &diagnostics);

    source.items = {&pattern, &diagnostics};
    menu.refresh();
    TEST_ASSERT_EQUAL_UINT(1, menu.highlight(0));
    TEST_ASSERT_TRUE(menu.highlightedItem() == &diagnostics);
    TEST_ASSERT_TRUE(menu.takeChanges().pages);
}

void test_refresh_closes_pages_opened_from_a_row_that_went_away() {
    MenuCommandItem disconnect(
        "disconnect", "Disconnect", kDisconnectCommand, MenuCommandItem::Style::Destructive);
    FakeChoice pattern("Default pattern", {"Auto", "Simple"}, 0);
    MenuCommandItem diagnostics("diagnostics", "Diagnostics", kDiagnosticsCommand);
    FakeSource source;
    source.items = {&disconnect, &pattern, &diagnostics};
    MenuPage page;
    page.addSource(source);
    MenuController menu;
    menu.open(page);
    menu.update(selectInput());
    menu.update(stepInput(1));
    TEST_ASSERT_EQUAL_UINT(2, menu.depth());

    source.items = {&diagnostics};
    menu.refresh();

    TEST_ASSERT_EQUAL_UINT(1, menu.depth());
    TEST_ASSERT_EQUAL_INT(1, pattern.cancels);
    TEST_ASSERT_TRUE(menu.highlightedItem() == &diagnostics);
    TEST_ASSERT_TRUE(pattern.commits.empty());
}

void test_sources_add_rows_in_registration_order_up_to_the_capacity() {
    MenuCommandItem pause("pause", "Pause", 1);
    MenuCommandItem settings("settings", "Settings", 2);
    FakeSource deviceSource;
    deviceSource.items = {&pause};
    FakeSource remoteSource;
    remoteSource.items = {&settings};
    MenuPage page;
    TEST_ASSERT_TRUE(page.addSource(deviceSource));
    TEST_ASSERT_TRUE(page.addSource(remoteSource));

    page.populate();
    TEST_ASSERT_EQUAL_UINT(2, page.count());
    TEST_ASSERT_TRUE(&page.item(0) == &pause);
    TEST_ASSERT_TRUE(&page.item(1) == &settings);
    TEST_ASSERT_EQUAL_UINT(1, page.indexOf(settings));

    MenuPage full;
    for (std::size_t index = 0; index < MenuPage::kMaxItems; ++index) {
        TEST_ASSERT_TRUE(full.add(pause));
    }
    TEST_ASSERT_FALSE(full.add(pause));
    TEST_ASSERT_EQUAL_UINT(MenuPage::kMaxItems, full.count());
}

void test_step_index_starts_at_either_end_without_a_current_index() {
    const std::size_t none = MenuPage::kNoItem;
    TEST_ASSERT_EQUAL_UINT(0, stepMenuIndex(none, 4, 1));
    TEST_ASSERT_EQUAL_UINT(1, stepMenuIndex(none, 4, 2));
    TEST_ASSERT_EQUAL_UINT(3, stepMenuIndex(none, 4, -1));
    TEST_ASSERT_EQUAL_UINT(0, stepMenuIndex(none, 4, -10));
    TEST_ASSERT_EQUAL_UINT(3, stepMenuIndex(1, 4, 10));
    TEST_ASSERT_TRUE(stepMenuIndex(none, 0, 1) == none);
}

void test_tiles_from_every_source_come_before_the_rows() {
    MenuCommandItem pause("pause", "Pause", 1);
    MenuCommandItem pullOut("pullout", "Pull out", 2);
    MenuCommandItem lock("lock", "Lock", 3);
    FakeChoice depthMode("Depth mode", {"Stroke / Depth", "Min / Max"}, 0);
    FakeChoice brightness("Brightness", fourOptions(), 1);
    FakeSource deviceSource;
    deviceSource.tiles = {&pause, &pullOut};
    deviceSource.items = {&depthMode};
    FakeSource remoteSource;
    remoteSource.tiles = {&lock};
    remoteSource.items = {&brightness};
    MenuPage page;
    page.addSource(deviceSource);
    page.addSource(remoteSource);

    MenuController menu;
    menu.open(page);
    TEST_ASSERT_EQUAL_UINT(5, page.count());
    TEST_ASSERT_EQUAL_UINT(3, page.tileCount());
    TEST_ASSERT_TRUE(&page.item(0) == &pause);
    TEST_ASSERT_TRUE(&page.item(1) == &pullOut);
    TEST_ASSERT_TRUE(&page.item(2) == &lock);
    TEST_ASSERT_TRUE(&page.item(3) == &depthMode);
    TEST_ASSERT_TRUE(&page.item(4) == &brightness);
    TEST_ASSERT_EQUAL_STRING("Run", pause.selectLabel());
    TEST_ASSERT_EQUAL_STRING("Change", depthMode.selectLabel());

    // The highlight starts on the first tile and moves on through the rows.
    TEST_ASSERT_TRUE(menu.highlightedItem() == &pause);
    TEST_ASSERT_TRUE(menu.highlightedTile() == &pause);
    menu.update(stepInput(2));
    TEST_ASSERT_TRUE(menu.highlightedTile() == &lock);
    menu.update(stepInput(1));
    TEST_ASSERT_TRUE(menu.highlightedItem() == &depthMode);
    TEST_ASSERT_NULL(menu.highlightedTile());

    page.populate();
    TEST_ASSERT_EQUAL_UINT(5, page.count());
    TEST_ASSERT_EQUAL_UINT(3, page.tileCount());

    MenuPage full;
    for (std::size_t index = 0; index < MenuPage::kMaxTiles; ++index) {
        TEST_ASSERT_TRUE(full.addTile(pause));
    }
    TEST_ASSERT_FALSE(full.addTile(pause));
    TEST_ASSERT_EQUAL_UINT(MenuPage::kMaxTiles, full.tileCount());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_open_starts_on_the_first_row_that_is_not_destructive);
    RUN_TEST(test_highlight_stops_at_the_first_and_last_row);
    RUN_TEST(test_back_on_the_root_page_closes_the_menu);
    RUN_TEST(test_command_rows_hand_their_command_to_the_host);
    RUN_TEST(test_tapping_a_row_highlights_and_selects_it);
    RUN_TEST(test_link_opens_a_page_and_back_returns_to_the_same_row);
    RUN_TEST(test_pages_open_no_deeper_than_the_maximum_depth);
    RUN_TEST(test_choice_popup_previews_options_and_back_cancels_the_preview);
    RUN_TEST(test_choice_popup_select_saves_the_option_and_closes);
    RUN_TEST(test_failed_popup_save_stays_open_on_the_stored_option);
    RUN_TEST(test_popup_without_a_matching_option_starts_without_a_highlight);
    RUN_TEST(test_inline_adjustment_previews_and_saves_when_the_highlight_leaves);
    RUN_TEST(test_inline_adjustment_back_to_the_stored_option_saves_nothing);
    RUN_TEST(test_failed_inline_save_cancels_the_preview_and_reports_it);
    RUN_TEST(test_closing_the_menu_saves_a_pending_inline_adjustment);
    RUN_TEST(test_select_saves_a_pending_inline_adjustment_before_opening_the_options);
    RUN_TEST(test_refresh_keeps_the_highlight_on_the_same_row);
    RUN_TEST(test_refresh_closes_pages_opened_from_a_row_that_went_away);
    RUN_TEST(test_sources_add_rows_in_registration_order_up_to_the_capacity);
    RUN_TEST(test_step_index_starts_at_either_end_without_a_current_index);
    RUN_TEST(test_tiles_from_every_source_come_before_the_rows);
    return UNITY_END();
}
