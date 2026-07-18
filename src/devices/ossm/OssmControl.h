#pragma once

#include <cstdint>

#include "OssmClient.h"

namespace m5_redux {

struct OssmControlValues {
    int speed = 0;
    int stroke = 10;
    int depth = 10;
    int sensation = 50;
    int pattern = 0;
};

struct OssmControlAdjustments {
    std::int64_t speed = 0;
    std::int64_t stroke = 0;
    std::int64_t depth = 0;
    std::int64_t sensation = 0;
};

class OssmControl {
  public:
    explicit OssmControl(ossm::OssmClient& client);

    void resetDefaults();
    bool apply(const OssmControlAdjustments& adjustments);
    bool setPattern(int patternId);
    void stop();
    void handleReadinessLost();
    const OssmControlValues& values() const;

  private:
    static constexpr int kMinimum = 0;
    static constexpr int kMaximum = 100;

    ossm::OssmClient& client_;
    OssmControlValues values_{};

    static int adjustPercent(int value, std::int64_t adjustment);
};

}  // namespace m5_redux
