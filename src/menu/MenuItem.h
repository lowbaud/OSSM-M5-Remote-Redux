#pragma once

#include <cstddef>
#include <cstdint>

namespace m5_redux {

class MenuPage;
class MenuChoiceItem;

// Host-defined request handed out by MenuController::update(), such as opening another screen.
using MenuCommand = std::uint16_t;

// Moves an index through count entries, stopping at the first and last one. Without a valid
// current index, turning forward starts at the first entry and turning back at the last.
std::size_t stepMenuIndex(std::size_t current, std::size_t count, std::int32_t steps);

enum class MenuItemKind : std::uint8_t {
    Action,
    Link,
    Choice,
    Option,
};

struct MenuResult {
    enum class Type : std::uint8_t {
        None,
        // The row's value changed.
        Changed,
        // Saving failed and the row went back to the stored value.
        Failed,
        OpenPage,
        OpenChoice,
        // Closes the page holding the row after it finished, e.g. an option was picked.
        ClosePage,
        Command,
    };

    Type type = Type::None;
    MenuPage* page = nullptr;
    MenuChoiceItem* choice = nullptr;
    MenuCommand command = 0;

    static MenuResult changed() {
        return {Type::Changed};
    }

    static MenuResult failed() {
        return {Type::Failed};
    }

    static MenuResult openPage(MenuPage& page) {
        return {Type::OpenPage, &page};
    }

    static MenuResult openChoice(MenuChoiceItem& choice) {
        return {Type::OpenChoice, nullptr, &choice};
    }

    static MenuResult closePage() {
        return {Type::ClosePage};
    }

    static MenuResult sendCommand(MenuCommand command) {
        return {Type::Command, nullptr, nullptr, command};
    }
};

// A row on a menu page. Items are created once and live for the whole program; pages only point
// to them, so building a page never allocates.
class MenuItem {
  public:
    MenuItem(const char* id, const char* name);
    MenuItem(const MenuItem&) = delete;
    MenuItem& operator=(const MenuItem&) = delete;
    virtual ~MenuItem() = default;

    // Stable key for the saved row order, namespaced by source, e.g. "ossm.depthmode".
    const char* id() const;
    virtual const char* name() const;
    virtual MenuItemKind kind() const = 0;
    // Shown on the right side of the row; empty when the row has no value.
    virtual const char* valueText() const;
    // Marks rows that open another page or screen.
    virtual bool showsChevron() const;
    // Symbol drawn on action tiles, e.g. LV_SYMBOL_PAUSE; empty for none.
    const char* icon() const;
    void setIcon(const char* icon);
    // Label of the select button while the row is highlighted.
    virtual const char* selectLabel() const;
    // A page never starts on a destructive row, so a quick select cannot trigger it by accident.
    virtual bool destructive() const;
    // Inner knob turns while the row is highlighted. Returns true when the row changed.
    virtual bool adjust(std::int32_t steps);
    virtual MenuResult activate();
    virtual void highlighted();
    // Called when the highlight leaves the row, a page opens above it, or the menu closes.
    virtual MenuResult unhighlighted();

  private:
    const char* id_;
    const char* name_;
    const char* icon_ = "";
};

}  // namespace m5_redux
