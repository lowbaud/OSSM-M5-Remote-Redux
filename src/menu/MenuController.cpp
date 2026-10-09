#include "MenuController.h"

#include <algorithm>

namespace m5_redux {

void MenuController::open(MenuPage& root) {
    close();
    push(root);
}

void MenuController::close() {
    if (depth_ == 0) {
        return;
    }

    MenuItem* item = highlightedItem();
    if (item != nullptr) {
        noteResult(item->unhighlighted());
    }
    truncate(0);
}

bool MenuController::isOpen() const {
    return depth_ > 0;
}

void MenuController::refresh() {
    if (depth_ == 0) {
        return;
    }

    MenuItem* active = highlightedItem();
    for (std::size_t level = 0; level < depth_; ++level) {
        Frame& frame = frames_[level];
        MenuItem* previous = itemAt(frame);
        const std::size_t previousIndex = frame.highlight;
        frame.page->populate();
        if (previous == nullptr) {
            frame.highlight = kNoItem;
            continue;
        }

        frame.highlight = frame.page->indexOf(*previous);
        if (frame.highlight != kNoItem) {
            continue;
        }

        // The row went away; stay close to where it was.
        const std::size_t count = frame.page->count();
        frame.highlight = count == 0 ? kNoItem : std::min(previousIndex, count - 1);
        if (level + 1 < depth_) {
            truncate(level + 1);
        }
    }

    MenuItem* current = highlightedItem();
    if (current != active) {
        if (active != nullptr) {
            noteResult(active->unhighlighted());
        }
        if (current != nullptr) {
            current->highlighted();
        }
    }
    changes_.pages = true;
}

MenuEvent MenuController::update(const MenuInput& input) {
    if (depth_ == 0) {
        return {};
    }

    if (input.steps != 0) {
        moveHighlight(input.steps);
    }

    if (input.adjustSteps != 0) {
        MenuItem* item = highlightedItem();
        if (item != nullptr && item->adjust(input.adjustSteps)) {
            changes_.values = true;
        }
    }

    bool select = input.select;
    const Frame& top = frames_[depth_ - 1];
    if (input.tappedRow < top.page->count()) {
        if (input.tappedRow != top.highlight) {
            setHighlight(input.tappedRow);
        }
        select = true;
    }

    if (select) {
        MenuItem* item = highlightedItem();
        if (item != nullptr) {
            const MenuEvent event = apply(item->activate());
            if (event.type != MenuEvent::Type::None) {
                return event;
            }
        }
    }

    if (input.back) {
        if (depth_ > 1) {
            pop(true);
        } else {
            close();
            return {MenuEvent::Type::Closed};
        }
    }

    return {};
}

MenuChanges MenuController::takeChanges() {
    const MenuChanges changes = changes_;
    changes_ = {};
    return changes;
}

std::size_t MenuController::depth() const {
    return depth_;
}

MenuPage& MenuController::page(std::size_t level) const {
    return *frames_[level].page;
}

bool MenuController::popupOpen() const {
    return depth_ > 0 && frames_[depth_ - 1].page == &optionsPage_;
}

std::size_t MenuController::highlight(std::size_t level) const {
    return frames_[level].highlight;
}

MenuItem* MenuController::highlightedItem() const {
    return depth_ == 0 ? nullptr : itemAt(frames_[depth_ - 1]);
}

void MenuController::push(MenuPage& page) {
    if (depth_ >= frames_.size()) {
        return;
    }

    MenuItem* parent = highlightedItem();
    if (parent != nullptr) {
        noteResult(parent->unhighlighted());
    }

    page.populate();
    Frame& frame = frames_[depth_++];
    frame.page = &page;
    frame.highlight = page.initialHighlight();
    MenuItem* item = itemAt(frame);
    if (item != nullptr) {
        item->highlighted();
    }
    changes_.pages = true;
}

void MenuController::pop(bool dismissed) {
    if (depth_ == 0) {
        return;
    }

    MenuItem* item = highlightedItem();
    if (item != nullptr) {
        noteResult(item->unhighlighted());
    }

    Frame& frame = frames_[--depth_];
    if (dismissed) {
        frame.page->dismissed();
    }
    frame = {};

    MenuItem* parent = highlightedItem();
    if (parent != nullptr) {
        parent->highlighted();
    }
    changes_.pages = true;
}

void MenuController::truncate(std::size_t depth) {
    while (depth_ > depth) {
        Frame& frame = frames_[--depth_];
        frame.page->dismissed();
        frame = {};
    }
    changes_.pages = true;
}

void MenuController::moveHighlight(std::int32_t steps) {
    const Frame& frame = frames_[depth_ - 1];
    const std::size_t count = frame.page->count();
    if (count == 0) {
        return;
    }

    const std::size_t next = stepMenuIndex(frame.highlight, count, steps);
    if (next != frame.highlight) {
        setHighlight(next);
    }
}

void MenuController::setHighlight(std::size_t index) {
    Frame& frame = frames_[depth_ - 1];
    MenuItem* previous = itemAt(frame);
    if (previous != nullptr) {
        noteResult(previous->unhighlighted());
    }

    frame.highlight = index;
    MenuItem* item = itemAt(frame);
    if (item != nullptr) {
        item->highlighted();
    }
    changes_.highlight = true;
}

MenuEvent MenuController::apply(const MenuResult& result) {
    switch (result.type) {
        case MenuResult::Type::None:
            break;
        case MenuResult::Type::Changed:
        case MenuResult::Type::Failed:
            noteResult(result);
            if (result.type == MenuResult::Type::Failed &&
                frames_[depth_ - 1].page->resetHighlightOnFailure()) {
                setHighlight(frames_[depth_ - 1].page->initialHighlight());
            }
            break;
        case MenuResult::Type::OpenPage:
            if (result.page != nullptr) {
                push(*result.page);
            }
            break;
        case MenuResult::Type::OpenChoice:
            // One options page serves every choice, so a choice cannot open from inside it.
            if (result.choice != nullptr && !popupOpen()) {
                optionsPage_.configure(*result.choice);
                push(optionsPage_);
            }
            break;
        case MenuResult::Type::ClosePage:
            if (depth_ > 1) {
                pop(false);
                changes_.values = true;
            }
            break;
        case MenuResult::Type::Command:
            return {MenuEvent::Type::Command, result.command};
    }
    return {};
}

void MenuController::noteResult(const MenuResult& result) {
    if (result.type == MenuResult::Type::Changed) {
        changes_.values = true;
    } else if (result.type == MenuResult::Type::Failed) {
        changes_.values = true;
        changes_.saveFailed = true;
    }
}

MenuItem* MenuController::itemAt(const Frame& frame) {
    if (frame.page == nullptr || frame.highlight >= frame.page->count()) {
        return nullptr;
    }
    return &frame.page->item(frame.highlight);
}

}  // namespace m5_redux
