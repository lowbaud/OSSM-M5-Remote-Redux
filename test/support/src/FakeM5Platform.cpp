#include "FakeM5Platform.h"

#include "platform/M5Platform.h"

namespace m5_redux::m5_platform {

void powerOff() {
    ++test_support::powerOffCalls;
}

}  // namespace m5_redux::m5_platform
