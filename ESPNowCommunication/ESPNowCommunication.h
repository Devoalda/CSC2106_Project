#include "MACaddr.h"
#ifndef ESP_NOW_COMMUNICATION_H
#define ESP_NOW_COMMUNICATION_H

#include <esp_now.h>
#include <PubSubClient.h>

#ifndef Arduino_h
#define Arduino_h
#include <Arduino.h>
#endif

#include <SensirionI2CScd4x.h>

#ifndef Wire_h
#define Wire_h
#include <Wire.h>
#endif

#define MAX_NODES 10
// Liligo
// #define I2C_SDA 46
// #define I2C_SCL 45

// M5 
#define I2C_SDA 26
#define I2C_SCL 25


// Struc for transmitting sensor data
struct SensorData {
  // uint8_t rootNodeAddress;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  float c02Data;
  float temperatureData;
  float humidityData;
};

// Connection Request Struct
struct Handshake {
  // 0 for request, 1 for reply
  uint8_t requestType;
  // 1 if connected to master, 0 otherwise
  uint8_t isConnectedToMaster;
  // number of hops to master
  uint8_t numberOfHopsToMaster;
};


// Extern variables
extern esp_now_peer_info_t peerInfo;
extern esp_now_peer_num_t peer_num;
extern uint8_t isConnectedToMaster;
extern uint8_t numberOfHopsToMaster;
//extern SensirionI2CScd4x scd4x;
extern SensorData sensorData;
extern Handshake msg;
extern unsigned int failedMessageCount; 
//extern const unsigned int MAX_FAILED_MESSAGES; 



void addPeerToPeerList(const uint8_t *macAddr);
void sendToAllPeers(const SensorData &sensorData);
void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen);
void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status);
void broadcast(const Handshake &msg);
float getRandomFloat(float min, float max);
String getRandomFloatAsString(float min, float max);
void printUint16Hex(uint16_t value);
void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2);
void espnowSetup();
void espnowUninit();
void espnowLoop();

#endif