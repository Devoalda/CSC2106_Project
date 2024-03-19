#include "MACaddr.h"
#include <Arduino.h>
#include "ESPNowCommunication.h"

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Setup code here
  setupMACaddr();
  espnowSetup();
  Serial.println("setup completed");
}

void loop() {
  // Loop code here
  espnowLoop();
}
