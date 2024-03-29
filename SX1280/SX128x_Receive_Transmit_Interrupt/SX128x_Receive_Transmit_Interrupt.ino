#include <WiFi.h>
#include "LoraMesh.h"


void setup() {
  Serial.begin(115200);
  delay(1000);

  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  Serial.println("Obtaining the MAC address");
  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Print MAC address
  Serial.print("MAC Address: ");

  uint8_t macAddressBytes[6];
  parseMacAddress(WiFi.macAddress(), macAddressBytes);
  // Format the MAC address and store it in sensorData.MACaddr
  formatMacAddress(macAddressBytes, selfAddr, MAX_MAC_LENGTH);
  // formatMacAddress(macAddressBytes, drm.MACaddr, MAX_MAC_LENGTH);

  Serial.println(selfAddr);

  // Disconnect from WiFi
  WiFi.disconnect();

  setupLoRa();
  setupSensor();
}


void loop() {
  // loopESP();
  loopLoRa();
}
