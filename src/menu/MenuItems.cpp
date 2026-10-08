#include "MenuItems.h"

namespace m5_redux {

MenuCommandItem::MenuCommandItem(const char* id, const char* name, MenuCommand command, Style style)
    : MenuItem(id, name), command_(command), style_(style) {}

MenuItemKind MenuCommandItem::kind() const {
    return MenuItemKind::Action;
}

bool MenuCommandItem::showsChevron() const {
    return style_ == Style::OpensScreen;
}

bool MenuCommandItem::wideTile() const {
    return style_ == Style::OpensScreen;
}

const char* MenuCommandItem::selectLabel() const {
    return style_ == Style::OpensScreen ? "Open" : "Select";
}

bool MenuCommandItem::destructive() const {
    return style_ == Style::Destructive;
}

MenuResult MenuCommandItem::activate() {
    return MenuResult::sendCommand(command_);
}

MenuLinkItem::MenuLinkItem(const char* id, const char* name, MenuPage& page)
    : MenuItem(id, name), page_(page) {}

MenuItemKind MenuLinkItem::kind() const {
    return MenuItemKind::Link;
}

bool MenuLinkItem::showsChevron() const {
    return true;
}

bool MenuLinkItem::wideTile() const {
    return true;
}

const char* MenuLinkItem::selectLabel() const {
    return "Open";
}

MenuResult MenuLinkItem::activate() {
    return MenuResult::openPage(page_);
}

MenuChoiceItem::MenuChoiceItem(const char* id, const char* name, SelectAction selectAction)
    : MenuItem(id, name), selectAction_(selectAction) {}

void MenuChoiceItem::previewOption(std::size_t) {}

void MenuChoiceItem::cancelPreview() {}

bool MenuChoiceItem::showsLevel() const {
    return false;
}

std::size_t MenuChoiceItem::displayedOption() const {
    return pendingOption_ != kNoOption ? pendingOption_ : storedOption();
}

bool MenuChoiceItem::toggledOn() const {
    return optionCount() == 2 && displayedOption() == 1;
}

MenuItemKind MenuChoiceItem::kind() const {
    return MenuItemKind::Choice;
}

bool MenuChoiceItem::wideTile() const {
    return showsLevel();
}

const char* MenuChoiceItem::valueText() const {
    const std::size_t option = displayedOption();
    return option < optionCount() ? optionName(option) : "";
}

const char* MenuChoiceItem::selectLabel() const {
    return selectAction_ == SelectAction::NextOption ? "Toggle" : "Change";
}

bool MenuChoiceItem::adjust(std::size_t, std::int32_t steps) {
    const std::size_t count = optionCount();
    if (steps == 0 || count == 0) {
        return false;
    }

    const std::size_t current = displayedOption();
    const std::size_t option = stepMenuIndex(current, count, steps);
    if (option == current) {
        return false;
    }

    // Turning back to the stored option leaves nothing to save.
    pendingOption_ = option == storedOption() ? kNoOption : option;
    previewOption(option);
    return true;
}

MenuResult MenuChoiceItem::activate() {
    const MenuResult pending = commitPending();
    if (pending.type == MenuResult::Type::Failed) {
        return pending;
    }

    const std::size_t count = optionCount();
    if (count == 0) {
        return pending;
    }

    if (selectAction_ == SelectAction::OpenOptions) {
        return MenuResult::openChoice(*this);
    }

    // Wraps around, and starts at the first option when the stored value matches none.
    const std::size_t stored = storedOption();
    const std::size_t next = stored < count - 1 ? stored + 1 : 0;
    if (commitOption(next)) {
        return MenuResult::changed();
    }
    cancelPreview();
    return MenuResult::failed();
}

MenuResult MenuChoiceItem::unhighlighted() {
    return commitPending();
}

MenuResult MenuChoiceItem::commitPending() {
    const std::size_t option = pendingOption_;
    pendingOption_ = kNoOption;
    // The options can change while an adjustment waits, e.g. when the pattern catalog reloads.
    if (option >= optionCount()) {
        return {};
    }
    if (commitOption(option)) {
        return MenuResult::changed();
    }
    cancelPreview();
    return MenuResult::failed();
}

MenuOptionItem::MenuOptionItem() : MenuItem("", "") {}

void MenuOptionItem::bind(MenuChoiceItem& choice, std::size_t index) {
    choice_ = &choice;
    index_ = index;
}

bool MenuOptionItem::stored() const {
    return choice_ != nullptr && choice_->storedOption() == index_;
}

const char* MenuOptionItem::name() const {
    return choice_ != nullptr ? choice_->optionName(index_) : "";
}

MenuItemKind MenuOptionItem::kind() const {
    return MenuItemKind::Option;
}

MenuResult MenuOptionItem::activate() {
    if (choice_ == nullptr) {
        return {};
    }
    if (choice_->commitOption(index_)) {
        return MenuResult::closePage();
    }
    choice_->cancelPreview();
    return MenuResult::failed();
}

void MenuOptionItem::highlighted() {
    if (choice_ != nullptr) {
        choice_->previewOption(index_);
    }
}

}  // namespace m5_redux
