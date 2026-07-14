#pragma once

#include "OssmClient.h"

namespace m5_redux {

struct OssmConnectionEvents {
    bool becameConnected = false;
    bool becameReady = false;
    bool connectionFailed = false;
    bool readinessLost = false;
};

class OssmConnectionController {
  public:
    explicit OssmConnectionController(ossm::OssmClient& client);

    bool begin(const char* localDeviceName);
    bool connect(const NimBLEAddress& address);
    void disconnect();
    OssmConnectionEvents update();
    bool isReady() const;

  private:
    ossm::OssmClient& client_;
    bool initialized_ = false;
    bool wasConnected_ = false;
    bool wasReady_ = false;
    bool connectionPending_ = false;
    bool failurePending_ = false;

    void failConnection();
};

}  // namespace m5_redux
