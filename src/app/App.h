#pragma once

namespace m5_redux {
namespace app {

void begin();
void update();
void stopMotion();
void showMenu();
void activateDiagnosticsBack();
void activateDiagnosticsAction();
void showScan();
void cancelScan();
void connectSelectedScanDevice();
void cancelConnection();
void showOssmPatterns();
void cancelOssmPatterns();
void selectOssmPattern();

}  // namespace app
}  // namespace m5_redux
