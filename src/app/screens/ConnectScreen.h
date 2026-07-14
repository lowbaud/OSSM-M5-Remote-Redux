#pragma once

#include <cstdint>

#include "platform/RemoteInput.h"

namespace m5_redux {

enum class ConnectScreenAction : std::uint8_t {
    None,
    Cancel,
    Retry,
};

class ConnectScreen {
  public:
    void begin();
    void configure(const char* deviceName, const char* deviceAddress);
    void enter();
    void leave();
    ConnectScreenAction update(const RemoteInputEvents& events);

    void requestCancel();
    void connectionStarted();
    void connectionEstablished();
    void connectionFailed();

  private:
    static constexpr std::uint32_t kRetryDelayMs = 2000;

    ConnectScreenAction pendingAction_ = ConnectScreenAction::None;
    std::uint32_t retryAtMs_ = 0;
    bool retryPending_ = false;
};

}  // namespace m5_redux
