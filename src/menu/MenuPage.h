#pragma once

#include <array>
#include <cstddef>

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
    // Actions shown as one row of tiles above the other rows.
    static constexpr std::size_t kMaxTiles = 4;
    static constexpr std::size_t kNoItem = static_cast<std::size_t>(-1);

    MenuPage() = default;
    MenuPage(const MenuPage&) = delete;
    MenuPage& operator=(const MenuPage&) = delete;
    virtual ~MenuPage() = default;

    // Shown above popups; full-screen pages have none.
    virtual const char* title() const;
    // Sources are registered once at startup and add their rows in registration order.
    bool addSource(MenuSource& source);
    // Rebuilds the rows from the sources.
    virtual void populate();
    // Returns false when the page is full.
    bool add(MenuItem& item);
    // Adds a tile after the tiles added before it; tiles come before every other row, whichever
    // source adds them. Returns false when the tiles or the page are full.
    bool addTile(MenuItem& item);
    std::size_t count() const;
    // The first tileCount() items are tiles.
    std::size_t tileCount() const;
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
    std::array<MenuItem*, kMaxItems> items_{};
    std::size_t count_ = 0;
    std::size_t tileCount_ = 0;
    std::array<MenuSource*, kMaxSources> sources_{};
    std::size_t sourceCount_ = 0;
};

// The popup listing the options of a choice. One instance serves every choice.
class MenuOptionsPage : public MenuPage {
  public:
    void configure(MenuChoiceItem& choice);

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
