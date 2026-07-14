#include <Arduino.h>

#include "app/App.h"

void setup() {
    m5_redux::app::begin();
}

void loop() {
    m5_redux::app::update();
}
