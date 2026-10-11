#pragma once

#include "menu/MenuItem.h"

namespace m5_redux {

// Requests from menu rows that App carries out.
enum class AppMenuCommand : MenuCommand {
    ShowDiagnostics = 1,
    Disconnect,
    // Quick action tiles. Each runs its action and closes the menu.
    // Switches between stroke / depth and min / max, like the Depth mode setting.
    SwitchDepthMode,
    // Pauses, or resumes when paused.
    Pause,
    PullOut,
    // Starts a ramp by the ramp settings, or ends the running one.
    Ramp,
};

}  // namespace m5_redux
