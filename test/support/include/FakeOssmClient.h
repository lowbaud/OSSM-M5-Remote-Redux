#pragma once

#include <vector>

#include "devices/ossm/OssmControlClient.h"

namespace test_support {

enum class CommandKind { Speed, Range, Depth, Stroke, Sensation, Pattern, Stop };

struct Command {
    CommandKind kind;
    int value = 0;
    int secondValue = 0;
};

class FakeOssmClient : public ossm::OssmControlClient {
  public:
    bool ready = true;
    bool acceptSpeed = true;
    std::vector<Command> commands;

    bool isReady() const override {
        return ready;
    }
    bool setSpeed(int value) override {
        commands.push_back({CommandKind::Speed, value});
        return acceptSpeed;
    }
    void setMotionRange(int depth, int stroke) override {
        commands.push_back({CommandKind::Range, depth, stroke});
    }
    void setDepth(int value) override {
        commands.push_back({CommandKind::Depth, value});
    }
    void setStroke(int value) override {
        commands.push_back({CommandKind::Stroke, value});
    }
    void setSensation(int value) override {
        commands.push_back({CommandKind::Sensation, value});
    }
    void setPattern(int value) override {
        commands.push_back({CommandKind::Pattern, value});
    }
    void stop() override {
        commands.push_back({CommandKind::Stop});
    }
};

}  // namespace test_support
