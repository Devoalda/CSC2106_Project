#include <Arduino.h>
#include "MACaddr.h"
#include "ESPNowCommunication.h"
#include "LoraCommunication.h"
#include "ProtocolManager.h"

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 10000;  //the value is a number of milliseconds
bool isEspnow = true;

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

  startMillis = millis();  //initial start time
}

void loop() {
  Serial.println("Ben ben says hi! Failed Message Count: " + String(failedMessageCount));

  // Check if it's time to switch from ESP-NOW to LoRa or vice versa.
  if (failedMessageCount >= MAX_FAILED_MESSAGES) {
    // Switch protocol
    isEspnow = !isEspnow; // Toggle between ESP-NOW and LoRa.

    // Reset failed message count after switching.
    failedMessageCount = 0;

    // Log the switch.
    if (isEspnow) {
      Serial.println("Switched to ESP-NOW");
    } else {
      Serial.println("Switched to LoRa");
    }
  }

  // Execute protocol-specific loop.
  if (isEspnow) {
    Serial.println("Operating in ESP-NOW mode");
    espnowLoop();
  } else {
    Serial.println("Operating in LoRa mode");
    loraLoop();
  }
}

