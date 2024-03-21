#include <Arduino.h>
#include "MACaddr.h"
#include "ESPNowCommunication.h"
#include "LoraCommunication.h"

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
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (isEspnow) {
    Serial.println("ESPNow");
    if (currentMillis - startMillis >= period)  //test whether the period has elapsed
    {
      isEspnow = false;
    } else {
      espnowLoop();
    }
  } else {
    loraLoop();
  }
}
