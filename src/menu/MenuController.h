#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "MenuItem.h"
#include "MenuPage.h"

namespace m5_redux {

struct MenuInput {
    // Outer knobs move the highlight.
    std::int32_t steps = 0;
    // Inner knobs adjust the highlighted row, left to right.
    std::array<std::int32_t, kMenuKnobCount> knobSteps{};
    bool back = false;
    bool select = false;
    // A row touched on the top page; it is highlighted and selected.
    std::size_t tappedRow = MenuPage::kNoItem;
};

// What the view has to redraw since it last asked.
struct MenuChanges {
    // Pages opened, closed or were repopulated.
    bool pages = false;
    bool highlight = false;
    bool values = false;
    bool saveFailed = false;
};

struct MenuEvent {
    enum class Type : std::uint8_t {
        None,
        // Back on the first page closed the menu.
        Closed,
        Command,
    };

    Type type = Type::None;
    MenuCommand command = 0;
};

// Page stack, highlight and input routing for the menu, independent of how it is drawn.
class MenuController {
  public:
    static constexpr std::size_t kMaxDepth = 4;
    static constexpr std::size_t kNoItem = MenuPage::kNoItem;

    // Opens the menu on the root page and discards the previous page stack.
    void open(MenuPage& root);
    // Saves an inline adjustment on the highlighted row and dismisses every open page.
    void close();
    bool isOpen() const;
    // Repopulates the open pages after the device or settings changed. Rows that are still
    // present keep the highlight; pages opened from a row that went away close.
    void refresh();
    MenuEvent update(const MenuInput& input);
    MenuChanges takeChanges();
    // Without inline adjustment the inner knobs do nothing and rows change through select.
    void setInlineAdjustEnabled(bool enabled);

    // Level 0 is the root page; the last level is the one taking input.
    std::size_t depth() const;
    MenuPage& page(std::size_t level) const;
    bool isPopup(std::size_t level) const;
    // Highlighted row index on the level, or kNoItem.
    std::size_t highlight(std::size_t level) const;
    // Highlighted row on the top page, or nullptr.
    MenuItem* highlightedItem() const;

  private:
    struct Frame {
        MenuPage* page = nullptr;
        std::size_t highlight = kNoItem;
        bool popup = false;
    };

    std::array<Frame, kMaxDepth> frames_{};
    std::size_t depth_ = 0;
    MenuOptionsPage optionsPage_;
    MenuChanges changes_{};
    bool inlineAdjustEnabled_ = true;

    void push(MenuPage& page, bool popup);
    void pop(bool dismissed);
    // Dismisses the pages from the top down to, but not including, the given depth.
    void truncate(std::size_t depth);
    void moveHighlight(std::int32_t steps);
    void setHighlight(std::size_t index);
    MenuEvent apply(const MenuResult& result);
    void noteResult(const MenuResult& result);
    bool optionsPageOpen() const;
    static MenuItem* itemAt(const Frame& frame);
};

}  // namespace m5_redux
