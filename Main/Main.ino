#include <Arduino.h>
#include "ESPNowCommunication.h"

void setup() {
  // Setup code here
  espnowSetup();
  Serial.println("setup completed");
}

void loop() {
  // Loop code here
  espnowLoop();
}
