#pragma once

namespace m5_redux {
namespace app {

void begin();
void update();
void stopMotion();
void showSettings();
void activateSettingsBack();
void activateSettingsSelect();
void showScan();
void cancelScan();
void connectSelectedScanDevice();
void cancelConnection();
void showOssmPatterns();
void cancelOssmPatterns();
void selectOssmPattern();

}  // namespace app
}  // namespace m5_redux
