#include <Arduino.h>
#include "MACaddr.h"
#include "ESPNowCommunication.h"
#include "LoraCommunication.h"


void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Setup code here
  setupMACaddr();
  setupSensor();
  espnowSetup();
  loraSetup();
  Serial.println("setup completed");
}

void loop() {
  // Loop code here
  espnowLoop();
  // loraLoop();
}
