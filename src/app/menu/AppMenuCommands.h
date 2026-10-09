#pragma once

#include "menu/MenuItem.h"

namespace m5_redux {

// Requests from menu rows that App carries out.
enum class AppMenuCommand : MenuCommand {
    ShowDiagnostics = 1,
    Disconnect,
    // Switches between stroke / depth and min / max, like the Depth mode setting.
    SwitchDepthMode,
    // Quick action tiles; placeholders until the actions exist.
    Pause,
    PullOut,
    Lock,
};

}  // namespace m5_redux
