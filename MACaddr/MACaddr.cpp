#include "MACaddr.h"

char MACaddrG[MAX_MAC_LENGTH]; // Define the global variable
SensirionI2CScd4x scd4x;

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

void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
}

void setupSensor() {
Wire.begin(I2C_SDA, I2C_SCL);

        uint16_t error;
        char errorMessage[256];

        scd4x.begin(Wire);

        // stop potentially previously started measurement
        error = scd4x.stopPeriodicMeasurement();
        if (error) {
          Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
          errorToString(error, errorMessage, 256);
          Serial.println(errorMessage);
        }

        uint16_t serial0;
        uint16_t serial1;
        uint16_t serial2;
        error = scd4x.getSerialNumber(serial0, serial1, serial2);
        if (error) {
          Serial.print("Error trying to execute getSerialNumber(): ");
          errorToString(error, errorMessage, 256);
          Serial.println(errorMessage);
        } else {
          printSerialNumber(serial0, serial1, serial2);
        }

        // Start Measurement
        error = scd4x.startPeriodicMeasurement();
        if (error) {
          Serial.print("Error trying to execute startPeriodicMeasurement(): ");
          errorToString(error, errorMessage, 256);
          Serial.println(errorMessage);
        }
}

float getRandomFloat(float min, float max) {
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));
  return randomFloat;
}

int getRandomInt(int min, int max) {
  return min + (rand() % (max - min + 1));
}