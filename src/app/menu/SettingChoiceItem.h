#pragma once

#include <cstddef>

#include "diagnostics/Log.h"
#include "menu/MenuItems.h"
#include "settings/SettingsStore.h"

namespace m5_redux {

// Pushes the stored settings to the parts of the remote that use them.
using ApplySettingsFn = void (*)();

// A choice over one of the option tables in SettingsStore. A saved value reaches the rest of the
// remote through the shared apply function, so a setting needs no item class of its own.
template <typename Option, typename Value> class SettingChoiceItem : public MenuChoiceItem {
  public:
    struct Setting {
        std::size_t optionCount;
        const Option& (*option)(std::size_t index);
        std::size_t (*optionIndex)(Value value);
        // The option field holding the stored value, e.g. &BrightnessOption::level.
        Value Option::* value;
        Value (SettingsStore::*get)() const;
        bool (SettingsStore::*set)(Value value);
    };

    SettingChoiceItem(
        const char* id,
        const char* name,
        SettingsStore& settings,
        const Setting& setting,
        ApplySettingsFn applySettings)
        : MenuChoiceItem(id, name), settings_(settings), setting_(setting),
          applySettings_(applySettings) {}

    std::size_t optionCount() const override {
        return setting_.optionCount;
    }

    const char* optionName(std::size_t index) const override {
        return setting_.option(index).name;
    }

    std::size_t storedOption() const override {
        return setting_.optionIndex(storedValue());
    }

    bool commitOption(std::size_t index) override {
        if (!(settings_.*setting_.set)(optionValue(index))) {
            LOGE("menu", "Unable to save %s setting", name());
            return false;
        }
        applySettings_();
        return true;
    }

  protected:
    Value storedValue() const {
        return (settings_.*setting_.get)();
    }

    Value optionValue(std::size_t index) const {
        return setting_.option(index).*setting_.value;
    }

  private:
    SettingsStore& settings_;
    Setting setting_;
    ApplySettingsFn applySettings_;
};

}  // namespace m5_redux
