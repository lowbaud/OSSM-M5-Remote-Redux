#pragma once

namespace ossm {

// Hardware-independent command boundary used by the motion controller.
class OssmControlClient {
  public:
    virtual ~OssmControlClient() = default;
    virtual bool isReady() const = 0;
    virtual bool setSpeed(int speed) = 0;
    virtual void setMotionRange(int depth, int stroke) = 0;
    virtual void setDepth(int depth) = 0;
    virtual void setStroke(int stroke) = 0;
    virtual void setSensation(int sensation) = 0;
    virtual void setPattern(int patternId) = 0;
    virtual void stop() = 0;
};

}  // namespace ossm
