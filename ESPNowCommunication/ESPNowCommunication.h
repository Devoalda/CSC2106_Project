#include "MACaddr.h"
#ifndef ESP_NOW_COMMUNICATION_H
#define ESP_NOW_COMMUNICATION_H

#include <esp_now.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>

#define MAX_NODES 10
#define CONNECTION_ATTEMPTS_LIMIT 10
#define CONNECTION_TIMEOUT 30000

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
extern bool attemptingConnection;
extern unsigned long connectionAttemptStartTime;
extern int numConnectionAttempts;
extern esp_now_peer_info_t peerInfo;
extern esp_now_peer_num_t peer_num;
extern uint8_t isConnectedToMaster;
extern uint8_t numberOfHopsToMaster;
extern uint8_t healthCheckCount;
extern SensirionI2CScd4x scd4x;
extern SensorData sensorData;
extern Handshake msg;
extern char MACaddrG[MAX_MAC_LENGTH];

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
void espnowLoop();

#endif