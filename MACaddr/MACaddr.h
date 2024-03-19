#ifndef MAC_ADDRESS_H
#define MAC_ADDRESS_H

#include <WiFi.h>
#include <SensirionI2CScd4x.h>

#define MAX_MAC_LENGTH 18

#define I2C_SDA 46
#define I2C_SCL 45

extern char MACaddrG[MAX_MAC_LENGTH]; // Declare the global variable
extern SensirionI2CScd4x scd4x;

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength);
void parseMacAddress(String macAddress, uint8_t *macAddressBytes);
void setupMACaddr();
void setupSensor();
float getRandomFloat(float min, float max);
int getRandomInt(int min, int max);

#endif
