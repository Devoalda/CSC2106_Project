#ifndef MAC_ADDRESS_H
#define MAC_ADDRESS_H

#include <WiFi.h>

#define MAX_MAC_LENGTH 18

extern char MACaddrG[MAX_MAC_LENGTH]; // Declare the global variable

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength);
void parseMacAddress(String macAddress, uint8_t *macAddressBytes);
void setupMACaddr();

#endif
