#pragma once

namespace m5_redux {
namespace serial_console {

// Prints how to request the diagnostic log.
void begin();
// Handles serial commands and continues a log dump a few lines at a time, so a slow or absent
// serial host never stalls the UI or the Stop button.
void update();

}  // namespace serial_console
}  // namespace m5_redux
