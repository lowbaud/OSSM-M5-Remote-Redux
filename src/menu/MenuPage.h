#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "MenuItem.h"
#include "MenuItems.h"

namespace m5_redux {

class MenuPage;

// Adds rows to a page. Pages ask their sources again each time they open, so the rows follow
// the connected device and the enabled options.
class MenuSource {
  public:
    virtual ~MenuSource() = default;
    virtual void addItems(MenuPage& page) = 0;
};

class MenuPage {
  public:
    // Holds the default pattern options: every pattern plus Auto.
    static constexpr std::size_t kMaxItems = 40;
    static constexpr std::size_t kMaxSources = 4;
    static constexpr std::size_t kNoItem = static_cast<std::size_t>(-1);

    enum class Layout : std::uint8_t {
        // Rows with the value on the right, for settings.
        List,
        // A grid of tiles that stand out at a glance, for quick actions.
        Tiles,
    };

    explicit MenuPage(const char* title, Layout layout = Layout::List);
    MenuPage(const MenuPage&) = delete;
    MenuPage& operator=(const MenuPage&) = delete;
    virtual ~MenuPage() = default;

    virtual const char* title() const;
    Layout layout() const;
    // Sources are registered once at startup and add their rows in registration order.
    bool addSource(MenuSource& source);
    // Rebuilds the rows from the sources.
    virtual void populate();
    // Returns false when the page is full.
    bool add(MenuItem& item);
    std::size_t count() const;
    MenuItem& item(std::size_t index) const;
    std::size_t indexOf(const MenuItem& item) const;
    // The first row that is not destructive, or kNoItem when there is none.
    virtual std::size_t initialHighlight() const;
    // Whether a failed save moves the highlight back to initialHighlight().
    virtual bool resetHighlightOnFailure() const;
    // Called when the page closes without finishing, by Back or because the menu closed.
    virtual void dismissed();

  protected:
    void clear();

  private:
    const char* title_;
    Layout layout_;
    std::array<MenuItem*, kMaxItems> items_{};
    std::size_t count_ = 0;
    std::array<MenuSource*, kMaxSources> sources_{};
    std::size_t sourceCount_ = 0;
};

// The popup listing the options of a choice. One instance serves every choice.
class MenuOptionsPage : public MenuPage {
  public:
    MenuOptionsPage();

    void configure(MenuChoiceItem& choice);
    MenuChoiceItem* choice() const;

    const char* title() const override;
    void populate() override;
    // Starts on the stored option, or without a highlight when it matches none.
    std::size_t initialHighlight() const override;
    bool resetHighlightOnFailure() const override;
    void dismissed() override;

  private:
    MenuChoiceItem* choice_ = nullptr;
    std::array<MenuOptionItem, kMaxItems> options_;
};

}  // namespace m5_redux
