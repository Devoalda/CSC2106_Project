/***
LoRa mesh on LilyGo T3S3 with RadioLib

Consist of leaf nodes (this) and master node.
// Leaf nodes can be activated in 2 ways: passive and active.

// Passive activation: node wait for DiscoveryReplyMessage, upon receive, if in passive mode, 
//   update selfLevel and broadcast DiscoveryReplyMessage, after a threshold turn passive mode off.

// Active activation: node initiates by sending DiscoveryMessage, triggered by empty addrList, 
//   turns on active mode; other nodes receives the DiscoveryMessage broadcasts the DiscoveryReplyMessage; 
//   only nodes in active mode react to DiscoveryReplyMessage; after a threshold turn active mode off.

*/

#ifndef INCLUDE_BOARD
#define INCLUDE_BOARD
#include "boards.h"
#endif

#ifndef SENSOR_DATA
#define SENSOR_DATA
#include "sensorData.h"
#endif

#include <RadioLib.h>

#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

#define WAITING_THRESHOLD 10000
#define SENSOR_DATA_INTERVAL 10000

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// global timer
unsigned long timer;

unsigned long sensorTimer;

volatile bool isolated = true;

// flag to indicate that a sensor data packet to be sent
volatile bool txFlag = false;

// flag to indicate waiting for route reply packet
volatile bool waitForReply = false;

// flag to indicate that a sensor data packet was received
volatile bool rxFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// initiate to max integer value, to be set after discovery
int selfLevel = 2147483647;

typedef struct SensorDataReply {
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];
} SensorDataReply;

SensorDataReply sdr = {
  .requestType = DATA_REPLY_MESSAGE
};

typedef struct Node {
  char MACaddr[MAX_MAC_LENGTH];
  Node* next;
} Node;

Node* addrList = NULL;

typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;

DiscoveryMessage dm = { DISCOVERY_MESSAGE };

typedef struct DiscoveryReplyMessage {
  uint8_t requestType;
  int level;
  char MACaddr[MAX_MAC_LENGTH];
} DiscoveryReplyMessage;

DiscoveryReplyMessage drm;

typedef struct QueueNode {
  String data;
  QueueNode* next;
} QueueNode;

typedef struct Queue {
  QueueNode* dataToSend;
  QueueNode* dataReceived;
} Queue;

Queue queue;

void addDataToSend(String data) {
  QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
  if (newNode == NULL) {
    // Handle memory allocation failure
    return;
  }

  newNode->data = data;
  newNode->next = NULL;

  if (queue.dataToSend == NULL) {
    queue.dataToSend = newNode;
    return;
  }

  QueueNode* last = queue.dataToSend;
  while (last->next != NULL) {
    last = last->next;
  }

  last->next = newNode;
}

void addDataReceived(String data) {
  QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
  if (newNode == NULL) {
    // Handle memory allocation failure
    return;
  }

  newNode->data = data;
  newNode->next = NULL;

  if (queue.dataReceived == NULL) {
    queue.dataReceived = newNode;
    return;
  }

  QueueNode* last = queue.dataReceived;
  while (last->next != NULL) {
    last = last->next;
  }

  last->next = newNode;
}

void removeDataToSend() {
  if (queue.dataToSend == NULL) {
    // The list is already empty
    return;
  }
  QueueNode* tempNode = queue.dataToSend;
  queue.dataToSend = queue.dataToSend->next;
  free(tempNode);
}

void removeDataReceived() {
  if (queue.dataReceived == NULL) {
    // The list is already empty
    return;
  }
  QueueNode* tempNode = queue.dataReceived;
  queue.dataReceived = queue.dataReceived->next;
  free(tempNode);
}

bool isQueueEmpty(QueueNode* head) {
  return head == NULL;
}

bool isAddrListEmpty(Node* head) {
  return head == NULL;
}

void addToAddrList(Node** head, char newMAC[MAX_MAC_LENGTH]) {
  // Create a new node
  Node* newNode = (Node*)malloc(sizeof(Node));
  if (newNode == NULL) {
    // Handle memory allocation failure
    return;
  }

  // Copy the MAC address into the new node
  for (int i = 0; i < MAX_MAC_LENGTH; i++) {
    newNode->MACaddr[i] = newMAC[i];
  }

  // The new node is going to be the last node, so make its next as NULL
  newNode->next = NULL;

  // If the Linked List is empty, then make the new node as head
  if (*head == NULL) {
    *head = newNode;
    return;
  }

  // Else traverse till the last node
  Node* last = *head;
  while (last->next != NULL) {
    last = last->next;
  }

  // Change the next of last node
  last->next = newNode;
}  // addToAddrList

void removeFromAddrList(Node** head) {
  if (*head == NULL) {
    // The list is already empty
    return;
  }
  Node* tempNode = *head;  // Temporarily store the head node
  *head = (*head)->next;   // Change head to the next node
  free(tempNode);          // Free the old head node
}  // removeFromAddrList

void clearAddrList(Node** head) {
  Node* current = *head;
  Node* nextNode;

  while (current != NULL) {
    nextNode = current->next;  // Store reference to the next node
    free(current);             // Free the current node
    current = nextNode;        // Move to the next node
  }

  *head = NULL;  // After all nodes are deleted, head should be NULL
}  // clearList

// this function is called when a complete packet
// is received or sent by the module
void setFlag(void) {
  // check if the interrupt is enabled
  if (!enableInterrupt) {
    // TODO: store the received packet in queue
    return;
  }

  // if not triggered by packet sending, means a packet is received
  if (txFlag == false) {
    rxFlag = true;
  }

  // if is triggered by packet sending, reset the flag
  if (txFlag == true) {
    txFlag == false;
  }
}  // setFlag

String serializeSDRToString() {
  String result;

  result.concat(sdr.requestType);
  result += ",";
  result += sdr.MACaddr;  // the MACaddr of the received

  return result;
}

SensorDataReply deserializeStringToSDR(String serialized) {
  SensorDataReply sdr;
  int firstCommaIndex = serialized.indexOf(',');

  // Convert and assign requestType
  sdr.requestType = (uint8_t)serialized.substring(0, firstCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized.substring(firstCommaIndex + 1);
  macAddrString.toCharArray(sdr.MACaddr, MAX_MAC_LENGTH);

  return sdr;
}

String serializeDMToString() {
  String result;

  result.concat(dm.requestType);
  result += ",";  // add for substring to work

  return result;
}

String serializeDRMToString() {
  String result;

  result.concat(drm.requestType);
  result += ",";
  result.concat(drm.level);
  result += ",";
  result += drm.MACaddr;

  return result;
}

DiscoveryReplyMessage deserializeStringToDRM(String serialized) {
  DiscoveryReplyMessage drm;
  int firstCommaIndex = serialized.indexOf(',');
  int secondCommaIndex = serialized.indexOf(',', firstCommaIndex + 1);

  // Convert and assign requestType
  drm.requestType = (uint8_t)serialized.substring(0, firstCommaIndex).toInt();

  // Convert and assign level
  drm.level = serialized.substring(firstCommaIndex + 1, secondCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized.substring(secondCommaIndex + 1);
  macAddrString.toCharArray(drm.MACaddr, MAX_MAC_LENGTH);

  return drm;
}

String serializeSensorDataToString() {
  String result;

  // Concatenate integer and float values directly.
  // Use String constructor or concat() for conversion from numeric to String.
  result.concat(sensorData.requestType);
  result += ",";  // Adding a separator for readability, can be omitted.
  result += sensorData.MACaddr;
  result += ",";
  result.concat(sensorData.c02Data);
  result += ",";
  result.concat(sensorData.temperatureData);
  result += ",";
  result.concat(sensorData.humidityData);

  return result;
}  // serializeSensorDataToString

SensorData deserializeStringToSensorData(String serialized) {
  SensorData sensorData;
  int firstCommaIndex = serialized.indexOf(',');
  int secondCommaIndex = serialized.indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = serialized.indexOf(',', secondCommaIndex + 1);
  int fourthCommaIndex = serialized.indexOf(',', thirdCommaIndex + 1);

  // Convert and assign requestType
  sensorData.requestType = (uint8_t)serialized.substring(0, firstCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized.substring(firstCommaIndex + 1, secondCommaIndex);
  macAddrString.toCharArray(sensorData.MACaddr, MAX_MAC_LENGTH);

  // Convert and assign c02Data
  sensorData.c02Data = serialized.substring(secondCommaIndex + 1, thirdCommaIndex).toFloat();

  // Convert and assign temperatureData
  sensorData.temperatureData = serialized.substring(thirdCommaIndex + 1, fourthCommaIndex).toFloat();

  // Convert and assign humidityData
  sensorData.humidityData = serialized.substring(fourthCommaIndex + 1).toFloat();

  return sensorData;
}  // deserializeStringToSensorData

void transmitData(String* data) {
  // disable interrupt service routine while transmitting
  enableInterrupt = false;

  // print a message to Serial to indicate transmission start
  Serial.println(F("[SX1280] Transmitting packet..."));

  // switch to transmit mode and send data
  int state = radio.transmit(*data);

  if (state == RADIOLIB_ERR_NONE) {
    // successfully sent
    Serial.println(F("[SX1280] Transmission successful!"));
    Serial.println(*data);
  } else {
    // failed to send
    Serial.print(F("[SX1280] Transmission failed, code "));
    Serial.println(state);
  }

  // after transmitting, switch back to receive mode
  radio.startReceive();
}  // transmitData

void setupLoRa() {
  // initialize SX1280 with default settings
  Serial.print(F("[SX1280] Initializing ... "));
  int state = radio.begin();

  drm.requestType = DISCOVERY_REPLY_MESSAGE;

  if (u8g2) {
    if (state != RADIOLIB_ERR_NONE) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, "Initializing: FAIL!");
      u8g2->sendBuffer();
    }
  }

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }

  //Set ANT Control pins
  radio.setRfSwitchPins(RADIO_RX_PIN, RADIO_TX_PIN);


  // T3 S3 V1.1 with PA Version Set output power to 3 dBm    !!Cannot be greater than 3dbm!!
  int8_t TX_Power = 3;
  if (radio.setOutputPower(TX_Power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println(F("Selected output power is invalid for this module!"));
    while (true)
      ;
  }

  // set carrier frequency to 2410.5 MHz
  if (radio.setFrequency(2400.0) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true)
      ;
  }

  // set bandwidth to 203.125 kHz
  if (radio.setBandwidth(203.125) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println(F("Selected bandwidth is invalid for this module!"));
    while (true)
      ;
  }

  // set spreading factor to 10
  if (radio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println(F("Selected spreading factor is invalid for this module!"));
    while (true)
      ;
  }

  // set coding rate to 6
  if (radio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println(F("Selected coding rate is invalid for this module!"));
    while (true)
      ;
  }
  // set the function that will be called
  // when packet transmission is finished
  radio.setDio1Action(setFlag);

  // start listening for LoRa packets
  Serial.print(F("[SX1280] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }
}  // setupLoRa

void processStringReceived(String str, String* msgToSend) {
  Serial.println(F("[SX1280] Received packet!"));

  // print data of the packet
  Serial.print(F("[SX1280] Data:\t\t"));
  Serial.println(str);
  // check msg type by spliting the str and check the first int
  int commaIndex = str.indexOf(',');              // Find the index of the first comma
  String msgType = str.substring(0, commaIndex);  // Extract the substring from the beginning to the comma
  if (msgType == String(DISCOVERY_MESSAGE)) {
    // reply with DISCOVERY_REPLY_MESSAGE if self isolated is false
    if (isolated == false) {
      *msgToSend = serializeDRMToString();
      txFlag = true;
    }
  } else if (msgType == String(DISCOVERY_REPLY_MESSAGE)) {
    // if self isolated is true, update addrList and level
    if (isolated == true) {
      DiscoveryReplyMessage received = deserializeStringToDRM(str);
      // if received level is lower than selfLevel-1, update selfLevel
      if (received.level < selfLevel - 1) {
        selfLevel = received.level - 1;
        clearAddrList(&addrList);
      } else if (received.level == selfLevel - 1) {
        addToAddrList(&addrList, received.MACaddr);
      }
    }

  } else if (msgType == String(DATA_MESSAGE)) {
    // if not isolated, forward the data message to the first addr in addrList
    // if addrList is empty, set isolated to true and send DISCOVERY_MESSAGE
    if (isAddrListEmpty(addrList)) {
      isolated = true;
    }

    if (isolated == false) {
      SensorData sd = deserializeStringToSensorData(str);
      memcpy(sd.MACaddr, addrList->MACaddr, MAX_MAC_LENGTH);
      *msgToSend = serializeSensorDataToString();
      txFlag = true;
    } else {
      timer = millis();
      *msgToSend = serializeDMToString();
      txFlag = true;
    }

  } else if (msgType == String(DATA_REPLY_MESSAGE)) {
    // if the mac address is not self address, ignore
    // if the mac address is self address, set waitForReply to false
    SensorDataReply received = deserializeStringToSDR(str);
    if (strcmp(received.MACaddr, sensorData.MACaddr) == 0) {
      waitForReply = false;
    }
  }

  if (u8g2) {
    u8g2->clearBuffer();
    char buf[256];
    u8g2->drawStr(0, 12, "Received OK!");
    snprintf(buf, sizeof(buf), "Data:%s", str);
    u8g2->drawStr(0, 26, buf);
    u8g2->sendBuffer();
  }
}  // processStringReceived

void loopLoRa() {
  String msgToSend;

  // check if the flag is set
  if (rxFlag) {
    // disable the interrupt service routine while
    // processing the data
    enableInterrupt = false;

    // reset flag
    rxFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      processStringReceived(str, &msgToSend);

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("[SX1280] CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("[SX1280] Failed, code "));
      Serial.println(state);
    }

    // put module back to listen mode
    radio.startReceive();

    // we're ready to receive more packets,
    // enable interrupt service routine
    enableInterrupt = true;
  }

  // TODO: check if there is any packet in the queue, if yes, process it

  unsigned long timeNow = millis();
  if (timeNow >= sensorTimer + SENSOR_DATA_INTERVAL) {
    sensorTimer = timeNow;
    if (getSensorReading() == true) {
      // TODO: add sensorData to queue instead
      msgToSend = serializeSensorDataToString();
      txFlag = true;
    }
  }

  if (txFlag) {
    enableInterrupt = false;

    txFlag = false;  // reset the flag

    // call to transmit data
    transmitData(&msgToSend);

    enableInterrupt = true;
  }
}  // loopLoRa
