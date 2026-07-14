#pragma once

#include <cstdint>

#include "settings/SettingsStore.h"

namespace m5_redux {

class BacklightController {
  public:
    void begin(BrightnessLevel brightness, std::uint32_t idleDimTimeoutSeconds);
    void update(std::uint32_t idleDurationMs);
    void setBrightnessLevel(BrightnessLevel brightness);
    void setIdleDimTimeoutSeconds(std::uint32_t seconds);

  private:
    static constexpr int kDimmedBrightnessPercent = 10;

    BrightnessLevel brightnessLevel_ = SettingsStore::kDefaultBrightnessLevel;
    std::uint32_t idleDimTimeoutSeconds_ = 0;
    bool dimmed_ = false;
    bool initialized_ = false;

    void applyNormalBrightness();
    void wake();
};

}  // namespace m5_redux
