#pragma once

namespace m5_redux {
namespace system_info {

// Describes why the chip last reset, for logs and the diagnostics screen.
const char* resetReasonName();
// True for crashes, watchdog resets and brownouts.
bool resetWasAbnormal();
// True when the reset keeps uninitialized RAM intact, so the previous session's log survives.
bool resetPreservesMemory();

}  // namespace system_info
}  // namespace m5_redux
