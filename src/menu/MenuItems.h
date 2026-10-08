#pragma once

#include <cstddef>
#include <cstdint>

#include "MenuItem.h"

namespace m5_redux {

// Hands a command to the host, e.g. to open the diagnostics screen or disconnect.
class MenuCommandItem : public MenuItem {
  public:
    enum class Style : std::uint8_t {
        Plain,
        // Shows a chevron and selects with "Open".
        OpensScreen,
        // Acts at once, e.g. Disconnect, so a page never starts on it.
        Destructive,
    };

    MenuCommandItem(
        const char* id, const char* name, MenuCommand command, Style style = Style::Plain);

    MenuItemKind kind() const override;
    bool showsChevron() const override;
    const char* selectLabel() const override;
    bool destructive() const override;
    MenuResult activate() override;

  private:
    MenuCommand command_;
    Style style_;
};

// Opens another page, e.g. Settings from the quick actions.
class MenuLinkItem : public MenuItem {
  public:
    MenuLinkItem(const char* id, const char* name, MenuPage& page);

    MenuItemKind kind() const override;
    bool showsChevron() const override;
    const char* selectLabel() const override;
    MenuResult activate() override;

  private:
    MenuPage& page_;
};

// A value picked from a fixed list of options. Select opens the options popup or moves to the
// next option; the inner knobs step through the options in place and save when the row loses
// the highlight.
class MenuChoiceItem : public MenuItem {
  public:
    static constexpr std::size_t kNoOption = static_cast<std::size_t>(-1);

    enum class SelectAction : std::uint8_t {
        OpenOptions,
        NextOption,
    };

    MenuChoiceItem(
        const char* id, const char* name, SelectAction selectAction = SelectAction::OpenOptions);

    virtual std::size_t optionCount() const = 0;
    virtual const char* optionName(std::size_t index) const = 0;
    // The stored option, or kNoOption when the stored value matches none of them.
    virtual std::size_t storedOption() const = 0;
    // Saves and applies the option. Returns false when it could not be saved.
    virtual bool commitOption(std::size_t index) = 0;
    // Applies an option without saving it, e.g. brightness while browsing.
    virtual void previewOption(std::size_t index);
    // Undoes previewOption() by applying the stored option again.
    virtual void cancelPreview();
    // Draws the options as a level bar instead of the option name, e.g. for brightness.
    virtual bool showsLevel() const;

    // The inline adjustment while one is pending, otherwise the stored option.
    std::size_t displayedOption() const;

    MenuItemKind kind() const override;
    const char* valueText() const override;
    const char* selectLabel() const override;
    bool adjust(std::size_t knob, std::int32_t steps) override;
    MenuResult activate() override;
    MenuResult unhighlighted() override;

  private:
    SelectAction selectAction_;
    std::size_t pendingOption_ = kNoOption;

    MenuResult commitPending();
};

// One row of the options popup, bound to an option of a choice when the popup opens.
class MenuOptionItem : public MenuItem {
  public:
    MenuOptionItem();

    void bind(MenuChoiceItem& choice, std::size_t index);
    // True for the option that is currently stored.
    bool stored() const;

    const char* name() const override;
    MenuItemKind kind() const override;
    MenuResult activate() override;
    void highlighted() override;

  private:
    MenuChoiceItem* choice_ = nullptr;
    std::size_t index_ = 0;
};

}  // namespace m5_redux
