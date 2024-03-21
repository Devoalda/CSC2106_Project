#ifndef LORACOMMUNICATION_H
#define LORACOMMUNICATION_H

#include <SensirionI2CScd4x.h>
#include <RadioLib.h>
#include <SPI.h>
#include "MACaddr.h"
#include "ProtocolManager.h"

#ifndef Arduino_h
#define Arduino_h
#include <Arduino.h>
#endif

#ifndef Wire_h
#define Wire_h
#include <Wire.h>
#endif

// pin definitions
#define RADIO_SCLK_PIN              5
#define RADIO_MISO_PIN              3
#define RADIO_MOSI_PIN              6
#define RADIO_CS_PIN                7
#define RADIO_DIO1_PIN              9
#define RADIO_RST_PIN               8
#define RADIO_BUSY_PIN              36

#define RADIO_RX_PIN                21
#define RADIO_TX_PIN                10

// LoRa module settings
#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

#define WAITING_THRESHOLD 1000      // 4 seconds reply waiting time
#define MAX_RETRY 3                 // 3 retries
#define SENSOR_DATA_INTERVAL 1000  // 10 seconds update interval

// global variables
extern SX1280 radio;
extern volatile uint8_t retry_fail_count;
extern volatile unsigned long replyTimer;
extern volatile bool replyTimerFlag;
extern volatile bool isolated;
extern volatile bool txStart;
extern volatile bool txDone;
extern volatile bool enableInterrupt;
extern volatile bool rxFlag;
extern volatile int selfLevel;

typedef struct LoraSensorData {
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  char SMACaddr[MAX_MAC_LENGTH]; // self mac
  float c02Data;
  float temperatureData;
  float humidityData;
  uint8_t randomNumber;
} LoraSensorData;
extern LoraSensorData loraSensorData;

typedef struct SensorDataReply {
  uint8_t requestType;
  uint8_t randomNumber;  // from the data packet
} SensorDataReply;
extern SensorDataReply sensorDataReply;

typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;
extern DiscoveryMessage discoveryMessage;

typedef struct DiscoveryReplyMessage {
  uint8_t requestType;
  int level;
  char MACaddr[MAX_MAC_LENGTH];  // selfAddr
} DiscoveryReplyMessage;
extern DiscoveryReplyMessage discoveryReplyMessage;

void loraSetup();
void loraLoop();

#endif