#include "MACaddr.h"

char MACaddrG[MAX_MAC_LENGTH]; // Define the global variable

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength) {
  // Function definition
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void parseMacAddress(String macAddress, uint8_t *macAddressBytes) {
  // Function definition
  sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &macAddressBytes[0], &macAddressBytes[1], &macAddressBytes[2],
         &macAddressBytes[3], &macAddressBytes[4], &macAddressBytes[5]);
}

void setupMACaddr() {
  // Function definition
  WiFi.mode(WIFI_STA);
  Serial.println("Obtaining MAC address...");

  Serial.print("MAC Address: ");
  uint8_t macAddressBytes[6];
  parseMacAddress(WiFi.macAddress(), macAddressBytes);
  formatMacAddress(macAddressBytes, MACaddrG, MAX_MAC_LENGTH);
  Serial.println(MACaddrG);

  WiFi.disconnect();
}
