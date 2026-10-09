#include "MenuPage.h"

#include <algorithm>

namespace m5_redux {

MenuPage::MenuPage(const char* title) : title_(title) {}

const char* MenuPage::title() const {
    return title_;
}

bool MenuPage::addSource(MenuSource& source) {
    if (sourceCount_ >= sources_.size()) {
        return false;
    }
    sources_[sourceCount_++] = &source;
    return true;
}

void MenuPage::populate() {
    clear();
    for (std::size_t index = 0; index < sourceCount_; ++index) {
        sources_[index]->addItems(*this);
    }
}

bool MenuPage::add(MenuItem& item) {
    if (count_ >= items_.size()) {
        return false;
    }
    items_[count_++] = &item;
    return true;
}

bool MenuPage::addTile(MenuItem& item) {
    if (tileCount_ >= kMaxTiles || count_ >= items_.size()) {
        return false;
    }
    for (std::size_t index = count_; index > tileCount_; --index) {
        items_[index] = items_[index - 1];
    }
    items_[tileCount_++] = &item;
    ++count_;
    return true;
}

std::size_t MenuPage::count() const {
    return count_;
}

std::size_t MenuPage::tileCount() const {
    return tileCount_;
}

MenuItem& MenuPage::item(std::size_t index) const {
    return *items_[index];
}

std::size_t MenuPage::indexOf(const MenuItem& item) const {
    for (std::size_t index = 0; index < count_; ++index) {
        if (items_[index] == &item) {
            return index;
        }
    }
    return kNoItem;
}

std::size_t MenuPage::initialHighlight() const {
    for (std::size_t index = 0; index < count_; ++index) {
        if (!items_[index]->destructive()) {
            return index;
        }
    }
    return kNoItem;
}

bool MenuPage::resetHighlightOnFailure() const {
    return false;
}

void MenuPage::dismissed() {}

void MenuPage::clear() {
    count_ = 0;
    tileCount_ = 0;
}

MenuOptionsPage::MenuOptionsPage() : MenuPage("") {}

void MenuOptionsPage::configure(MenuChoiceItem& choice) {
    choice_ = &choice;
}

MenuChoiceItem* MenuOptionsPage::choice() const {
    return choice_;
}

const char* MenuOptionsPage::title() const {
    return choice_ != nullptr ? choice_->name() : "";
}

void MenuOptionsPage::populate() {
    clear();
    if (choice_ == nullptr) {
        return;
    }

    const std::size_t count = std::min(choice_->optionCount(), options_.size());
    for (std::size_t index = 0; index < count; ++index) {
        options_[index].bind(*choice_, index);
        add(options_[index]);
    }
}

std::size_t MenuOptionsPage::initialHighlight() const {
    if (choice_ == nullptr) {
        return kNoItem;
    }
    const std::size_t stored = choice_->storedOption();
    return stored < count() ? stored : kNoItem;
}

bool MenuOptionsPage::resetHighlightOnFailure() const {
    return true;
}

void MenuOptionsPage::dismissed() {
    if (choice_ != nullptr) {
        choice_->cancelPreview();
    }
}

}  // namespace m5_redux
