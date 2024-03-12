/***
LoRa mesh on LilyGo T3S3 with RadioLib

Consist of leaf nodes (this) and master node.
Leaf nodes can be activated in 2 ways: passive and active.

Passive activation: node wait for DiscoveryReplyMessage, upon receive, if in passive mode, 
  update selfLevel and broadcast DiscoveryReplyMessage, after a threshold turn passive mode off.

Active activation: node initiates by sending DiscoveryMessage, triggered by empty addrList, 
  turns on active mode; other nodes receives the DiscoveryMessage broadcasts the DiscoveryReplyMessage; 
  only nodes in active mode react to DiscoveryReplyMessage; after a threshold turn active mode off.

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

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// flag to indicate active mode, default to false
volatile bool active = false;

// flag to indicate passive mode, default to true
volatile bool passive = true;

// flag to indicate that a sensor data packet to be sent
volatile bool txFlag = false;

// flag to indicate a route discovery packet was sent
volatile bool routeDiscoveryFlag = false;

// flag to indicate waiting for route reply packet
volatile bool waitingForRouteReply = false;

// flag to indicate that a sensor data packet was received
volatile bool rxFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// initate to max integer value, to be set after discovery
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

int isAddrListEmpty(Node* head) {
  return head == NULL;
}  // isAddrListEmpty

void addToAddrList(Node** head, uint8_t newMAC[MAX_MAC_LENGTH]) {
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
    if (u8g2) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, "Transmitting: KO!");
      u8g2->drawStr(0, 30, ("TX:" + *data).c_str());
      u8g2->sendBuffer();
    }
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

void loopLoRa() {
  // testing
  static unsigned long lastTransmitTime = 0;
  const unsigned long transmitInterval = 10000;  // Interval to transmit, in milliseconds (e.g., 10000ms = 10 seconds)

  // Get the current time
  unsigned long currentTime = millis();

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

    // uint32_t counter;
    // int state = radio.readData((uint8_t *)&counter, 4);

    // you can also read received data as byte array
    /*
          byte byteArr[8];
          int state = radio.readData(byteArr, 8);
        */

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      Serial.println(F("[SX1280] Received packet!"));

      // print data of the packet
      Serial.print(F("[SX1280] Data:\t\t"));
      Serial.println(str);

      // check msg type by spliting the str and check the first int
      int commaIndex = str.indexOf(',');                   // Find the index of the first comma
      String msgType = str.substring(0, commaIndex);  // Extract the substring from the beginning to the comma

      // Use the firstElement as needed
      Serial.println("message type: " + msgType);

      if (msgType == String(DISCOVERY_MESSAGE)) {
        // reply with DISCOVERY_REPLY_MESSAGE if self is connected

      } else if (msgType == String(DISCOVERY_REPLY_MESSAGE)) {
        // if self is in active mode (active == true), update addrList

      } else if (msgType == String(DATA_MESSAGE)) {
        // forward the data message according to addrList

      } else if (msgType == String(DATA_REPLY_MESSAGE)) {
        // if the mac address is not self address, ignore

      }
      
      // // print RSSI (Received Signal Strength Indicator)
      // Serial.print(F("[SX1280] RSSI:\t\t"));
      // Serial.print(radio.getRSSI());
      // Serial.println(F(" dBm"));

      // // print SNR (Signal-to-Noise Ratio)
      // Serial.print(F("[SX1280] SNR:\t\t"));
      // Serial.print(radio.getSNR());
      // Serial.println(F(" dB"));

      if (u8g2) {
        u8g2->clearBuffer();
        char buf[256];
        u8g2->drawStr(0, 12, "Received OK!");
        snprintf(buf, sizeof(buf), "Data:%s", str);
        u8g2->drawStr(0, 26, buf);
        // snprintf(buf, sizeof(buf), "RSSI:%.2f", radio.getRSSI());
        // u8g2->drawStr(0, 40, buf);
        // snprintf(buf, sizeof(buf), "SNR:%.2f", radio.getSNR());
        // u8g2->drawStr(0, 54, buf);
        u8g2->sendBuffer();
      }

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

  // Simulate triggering data sending
  // Check if it's time to transmit
  if (currentTime - lastTransmitTime >= transmitInterval) {
    lastTransmitTime = currentTime;  // Update the last transmit time
    txFlag = true;                   // Set the flag to trigger transmission
  }

  if (txFlag) {
    enableInterrupt = false;

    txFlag = false;  // reset the flag

    sensorData.c02Data += 0.1;

    msgToSend = serializeSensorDataToString();

    // call to transmit data
    transmitData(&msgToSend);

    enableInterrupt = true;
  }
}  // loopLoRa
