#include "MenuItem.h"

namespace m5_redux {

std::size_t stepMenuIndex(std::size_t current, std::size_t count, std::int32_t steps) {
    if (count == 0) {
        return current;
    }

    const std::int64_t last = static_cast<std::int64_t>(count) - 1;
    std::int64_t next = 0;
    if (current < count) {
        next = static_cast<std::int64_t>(current) + steps;
    } else {
        next = steps > 0 ? steps - 1 : last + 1 + steps;
    }
    if (next < 0) {
        return 0;
    }
    return static_cast<std::size_t>(next > last ? last : next);
}

MenuItem::MenuItem(const char* id, const char* name) : id_(id), name_(name) {}

const char* MenuItem::id() const {
    return id_;
}

const char* MenuItem::name() const {
    return name_;
}

const char* MenuItem::valueText() const {
    return "";
}

bool MenuItem::showsChevron() const {
    return false;
}

const char* MenuItem::icon() const {
    return icon_;
}

void MenuItem::setIcon(const char* icon) {
    icon_ = icon;
}

const char* MenuItem::selectLabel() const {
    return "Select";
}

bool MenuItem::destructive() const {
    return false;
}

bool MenuItem::running() const {
    return false;
}

bool MenuItem::adjust(std::int32_t) {
    return false;
}

MenuResult MenuItem::activate() {
    return {};
}

void MenuItem::highlighted() {}

MenuResult MenuItem::unhighlighted() {
    return {};
}

}  // namespace m5_redux
