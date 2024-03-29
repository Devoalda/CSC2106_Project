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

#include <RadioLib.h>
#include <SensirionI2CScd4x.h>

#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

#define WAITING_THRESHOLD 1000      // 4 seconds reply waiting time
#define MAX_RETRY 3                 // 3 retries
#define SENSOR_DATA_INTERVAL 1000  // 10 seconds update interval

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

volatile uint8_t retry_fail_count = 0;

// global timer
volatile unsigned long replyTimer;

volatile bool replyTimerFlag = false;

volatile bool isolated = true;

// flag to indicate that a sensor data packet to be sent, for identifying the callback is triggered by transmit
volatile bool txStart = false;

volatile bool txDone = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// flag to indicate to call radio.startReceive()
volatile bool rxFlag = false;

// initiate to max integer value, to be set after discovery
volatile int selfLevel = 2147483647;

// Define the maximum length of the MAC address
const int MAX_MAC_LENGTH = 18;  // For example, a MAC address is usually 17 characters long (including colons) plus 1 for null terminator

char selfAddr[MAX_MAC_LENGTH];

// Struc for transmitting sensor data
typedef struct SensorData {
  // uint8_t rootNodeAddress;
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  char SMACaddr[MAX_MAC_LENGTH]; // self mac
  float c02Data;
  float temperatureData;
  float humidityData;
  uint8_t randomNumber;
} SensorData;

SensorData sensorData = {
  .requestType = 2
};

uint16_t error;
char errorMessage[256];
uint16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
bool isDataReady = false;

float getRandomFloat(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  // char buffer[10];                     // Adjust the size as needed
  // dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  // String floatString = String(buffer);

  return randomFloat;
}

// Function to parse a String MAC address to uint8_t arra

String getRandomFloatAsString(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  char buffer[10];                     // Adjust the size as needed
  dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  String floatString = String(buffer);

  return floatString;
}

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

// Function to parse a String MAC address to uint8_t array
void parseMacAddress(String macAddress, uint8_t *macAddressBytes) {
  sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &macAddressBytes[0], &macAddressBytes[1], &macAddressBytes[2],
         &macAddressBytes[3], &macAddressBytes[4], &macAddressBytes[5]);
}

int getRandomInt(int min, int max) {
  return min + (rand() % (max - min + 1));
}


SensirionI2CScd4x scd4x;

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

bool getSensorReading() {
  error = scd4x.getDataReadyFlag(isDataReady);
  if (error) {
    Serial.print("Error trying to execute getDataReadyFlag(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    // If error, use random data
    sensorData.c02Data = getRandomFloat(100.0, 1000.0);
    sensorData.temperatureData = getRandomFloat(0.0, 40.0);
    sensorData.humidityData = getRandomFloat(90.0, 1030.0);
  } else if (!isDataReady) {
    // If data not ready, return
    return false;
  } else {
    // Read sensor measurement
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      // If error, use random data
      sensorData.c02Data = getRandomFloat(100.0, 1000.0);
      sensorData.temperatureData = getRandomFloat(0.0, 40.0);
      sensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else if (co2 == 0) {
      Serial.println("Invalid sample detected, skipping.");
      // If invalid sample, use random data
      sensorData.c02Data = getRandomFloat(100.0, 1000.0);
      sensorData.temperatureData = getRandomFloat(0.0, 40.0);
      sensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else {
      // Assign sensor data to sensorData structure
      sensorData.c02Data = co2;
      sensorData.temperatureData = temperature;
      sensorData.humidityData = humidity;
    }
  }
  return true;
}

typedef struct SensorDataReply {
  uint8_t requestType;
  uint8_t randomNumber;  // from the data packet
} SensorDataReply;

SensorDataReply sdr = {
  .requestType = DATA_REPLY_MESSAGE
};

typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;

DiscoveryMessage dm = { DISCOVERY_MESSAGE };

typedef struct DiscoveryReplyMessage {
  uint8_t requestType;
  int level;
  char MACaddr[MAX_MAC_LENGTH];  // selfAddr
} DiscoveryReplyMessage;

DiscoveryReplyMessage drm;

template<typename T>
class Queue {
public:
  struct Node {
    T data;
    Node* next;
  };

  int count = 0;

  Node* head = NULL;

  void addToLast(T data) {
    // Check if data already exists in the queue
    Node* current = head;
    while (current != NULL) {
      if (current->data == data) {
        // Data already exists, do not add
        return;
      }
      current = current->next;
    }

    // Data does not exist in the queue, add it
    Node* newNode = new Node;
    newNode->data = data;
    newNode->next = NULL;

    if (head == NULL) {
      head = newNode;
      count = 1;
      return;
    }

    Node* last = head;
    while (last->next != NULL) {
      last = last->next;
    }

    last->next = newNode;
    count++;
  }

  void addToFirst(T data) {
    // Check if data already exists in the queue
    Node* current = head;
    while (current != NULL) {
      if (current->data == data) {
        // Data already exists, do not add
        return;
      }
      current = current->next;
    }

    // Data does not exist in the queue, add it
    Node* newNode = new Node;
    newNode->data = data;
    newNode->next = head;
    head = newNode;
    count++;
  }

  void removeFromFirst() {
    if (head == NULL) {
      // The list is already empty
      return;
    }
    Node* tempNode = head;
    head = head->next;
    delete tempNode;
    count--;
  }

  bool isEmpty() {
    return head == NULL;
  }

  void clear() {
    while (head != NULL) {
      removeFromFirst();
    }
    count = 0;
  }

  bool removeIfMatches(std::function<bool(T)> condition) {
    Node* current = head;
    Node* prev = NULL;
    while (current != NULL) {
      if (condition(current->data)) {
        if (prev == NULL) {
          head = current->next;
        } else {
          prev->next = current->next;
        }
        delete current;
        count--;
        return true;
      }
      prev = current;
      current = current->next;
    }
    return false;
  }
};

Queue<String> dataToSend;
Queue<String> dataReceived;
Queue<String> dataSending;
Queue<String> addrList;

// this function is called when a complete packet
// is received or sent by the module
void setFlag(void) {
  // if not transmitting, means triggered by packet receiving
  // if multiple packet is received at the same time when the later one
  // is received before the prior one is read and stored, the later one
  // will not be captured!
  if (txStart == false && enableInterrupt == true) {
    enableInterrupt = false;
    rxFlag = true;
  } else if (txStart == true) {
    // end of sending
    txStart = false;
    txDone = true;
    // Serial.println("Message sending completed");
  } else {
    // Serial.println("Data missing!");
    return;
  }
}  // setFlag

String serializeSDRToString() {
  String result;

  result.concat(sdr.requestType);
  result += ",";
  result.concat(sdr.randomNumber);

  return result;
}

SensorDataReply deserializeStringToSDR(String* serialized) {
  SensorDataReply sdr;
  int firstCommaIndex = serialized->indexOf(',');

  // Convert and assign requestType
  sdr.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Convert and assign randomNumber
  sdr.randomNumber = serialized->substring(firstCommaIndex + 1).toInt();

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
  result += selfAddr;

  return result;
}

DiscoveryReplyMessage deserializeStringToDRM(String* serialized) {
  DiscoveryReplyMessage drm;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);

  // Convert and assign requestType
  drm.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Convert and assign level
  drm.level = serialized->substring(firstCommaIndex + 1, secondCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized->substring(secondCommaIndex + 1);
  macAddrString.toCharArray(drm.MACaddr, MAX_MAC_LENGTH);

  return drm;
}

String serializeSensorDataToString(SensorData* data) {
  String result;

  // Concatenate integer and float values directly.
  // Use String constructor or concat() for conversion from numeric to String.
  result.concat(data->requestType);
  result += ",";  // Adding a separator for readability, can be omitted.
  result += data->MACaddr;
  result += ",";
  result += data->SMACaddr;
  result += ",",
  result.concat(data->c02Data);
  result += ",";
  result.concat(data->temperatureData);
  result += ",";
  result.concat(data->humidityData);
  result += ",";
  result.concat(data->randomNumber);

  return result;
}  // serializeSensorDataToString

SensorData deserializeStringToSensorData(String* serialized) {
  SensorData sensorData;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = serialized->indexOf(',', secondCommaIndex + 1);
  int fourthCommaIndex = serialized->indexOf(',', thirdCommaIndex + 1);
  int fifthCommaIndex = serialized->indexOf(',', fourthCommaIndex + 1);
  int sixthCommaIndex = serialized->indexOf(',', fifthCommaIndex + 1);

  // Convert and assign requestType
  sensorData.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Assign MACaddr (receiver)
  String macAddrString = serialized->substring(firstCommaIndex + 1, secondCommaIndex);
  macAddrString.toCharArray(sensorData.MACaddr, MAX_MAC_LENGTH);

  // sender's mac addr
  String sMacAddrString = serialized->substring(secondCommaIndex + 1, thirdCommaIndex);
  sMacAddrString.toCharArray(sensorData.SMACaddr, MAX_MAC_LENGTH);

  // Convert and assign c02Data
  sensorData.c02Data = serialized->substring(thirdCommaIndex + 1, fourthCommaIndex).toFloat();

  // Convert and assign temperatureData
  sensorData.temperatureData = serialized->substring(fourthCommaIndex + 1, fifthCommaIndex).toFloat();

  // Convert and assign humidityData
  sensorData.humidityData = serialized->substring(fifthCommaIndex + 1, sixthCommaIndex).toFloat();

  sensorData.randomNumber = serialized->substring(sixthCommaIndex + 1).toFloat();

  return sensorData;
}  // deserializeStringToSensorData

void transmitData(String* data) {
  // Serial.println(F("Transmitting packet..."));

  // if its isolated and not sending the discovery message
  if (isolated == true && data->charAt(0) != '0') {
    // Serial.println("isolated, message not Discovery Message, do nothing");
    return;
  }

  // if waiting for reply, and data is Data message, do nothing
  if (replyTimerFlag == true && data->charAt(0) == '2') {
    // Serial.println("Waiting for prior message reply, hold on message sending");
    return;
  }

  // if message is DataMessage, add first addr
  if (data->charAt(0) == '2') {
    const char* addrChar = addrList.head->data.c_str();
    SensorData sd = deserializeStringToSensorData(data);
    strncpy(sd.MACaddr, addrChar, MAX_MAC_LENGTH);
    *data = serializeSensorDataToString(&sd);
    // Serial.println("Data Message, addrList not empty, add first addr to the message");
  }

  // switch to transmit mode and send data
  int state = radio.startTransmit(*data);

  if (state == RADIOLIB_ERR_NONE) {
    txStart = true;
    // if data is of type SensorData or DiscoveryMessage, add to dataSending
    if (data->charAt(0) == '0' || data->charAt(0) == '2') {
      dataSending.addToLast(*data);
      // Serial.print("Putting msg at the TAIL of dataSending queue: ");
      // Serial.println(*data);

      // set replyTimer to current time if not set
      if (replyTimerFlag == false) {
        replyTimer = millis();
        replyTimerFlag = true;
        // Serial.println("Set replyTimerFlag to TRUE");
      }
    }
    // successfully sent
    dataToSend.removeFromFirst();
    // Serial.println(F("Transmission init successful! remove from dataToSendQueue"));

  } else {
    // failed to send
    // Serial.print(F("Transmission failed, code "));
    // Serial.println(state);
  }
}  // transmitData

void setupLoRa() {
  // initialize SX1280 with default settings
  Serial.print(F("LoRa Initializing ... "));
  int state = radio.begin();

  drm.requestType = DISCOVERY_REPLY_MESSAGE;

  if (u8g2) {
    if (state != RADIOLIB_ERR_NONE) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, "LoRa Initializing: FAIL!");
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
  if (radio.setFrequency(2410.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true)
      ;
  }

  // set bandwidth to 203.125 kHz
  if (radio.setBandwidth(812.5) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println(F("Selected bandwidth is invalid for this module!"));
    while (true)
      ;
  }

  // set spreading factor to 10
  if (radio.setSpreadingFactor(12) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println(F("Selected spreading factor is invalid for this module!"));
    while (true)
      ;
  }

  // set coding rate to 6
  if (radio.setCodingRate(7) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println(F("Selected coding rate is invalid for this module!"));
    while (true)
      ;
  }
  // set the function that will be called
  // when packet transmission is finished
  radio.setDio1Action(setFlag);

  // start listening for LoRa packets
  Serial.print(F("LoRa starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }

  if (u8g2) {
    u8g2->clearBuffer();
    u8g2->drawStr(0, 12, "LoRa Initializing: SUCCESS!");
    u8g2->drawStr(0, 24, "received: 0");
    u8g2->drawStr(0, 36, "toSend: 0");
    u8g2->drawStr(0, 48, "sending: 0");
    u8g2->sendBuffer();
  }

}  // setupLoRa

void processStringReceived(String* str) {
  // Serial.print(F("Process received msg: "));
  // Serial.println(*str);

  // check msg type by spliting the str and check the first int
  int commaIndex = str->indexOf(',');              // Find the index of the first comma
  String msgType = str->substring(0, commaIndex);  // Extract the substring from the beginning to the comma
  if (msgType == String(DISCOVERY_MESSAGE)) {
    // Serial.println("Message is DISCOVERY_MESSAGE");
    // reply with DISCOVERY_REPLY_MESSAGE if self isolated is false, else do nothing
    if (isolated == false) {
      drm.level = selfLevel;
      dataToSend.addToFirst(serializeDRMToString());
      // Serial.println("Self is not isolated, add discovery reply message to queue");
    }
    dataReceived.removeFromFirst();
    // Serial.println("DISCOVERY_MESSAGE processed, remove from dataReceived queue");

  } else if (msgType == String(DISCOVERY_REPLY_MESSAGE)) {
    // Serial.println("Message is DISCOVERY_REPLY_MESSAGE");
    // if self isolated is true, update addrList and level
    DiscoveryReplyMessage received = deserializeStringToDRM(str);
    // Serial.println(*str);

    // for debugging
    // if (strcmp(received.MACaddr, "dc:54:75:e4:6d:50") == 0) {
    //   dataReceived.removeFromFirst();
    //   Serial.println("DEBUG: ignore this device");
    //   return;
    // }


    // if received level is lower than selfLevel-1, update selfLevel
    if (received.level <= selfLevel - 1) {
      if (received.level < selfLevel - 1) {
        selfLevel = received.level + 1;
        addrList.clear();
        // Serial.print("adjust self level to ");
        // Serial.print(selfLevel);
        // Serial.println(", clear the addrList");
      }
      addrList.addToLast(String(received.MACaddr));
      // Serial.println("added address to addrList (if not exist): ");
      // Serial.println(String(received.MACaddr));

      isolated = false;
      // Serial.println("Set isolation to FALSE");

      replyTimerFlag = false;
      // Serial.println("Set replyTimerFlag to false");

      // remove discovery message from dataSending
      dataSending.removeIfMatches([](String data) {
        return data.charAt(0) == '0';
      });
    }


    dataReceived.removeFromFirst();
    // Serial.println("Discovery Reply Message processed, remove from dataReceived queue");

  } else if (msgType == String(DATA_MESSAGE)) {
    // Serial.println("Message is DATA_MESSAGE");
    // if not isolated, forward the data message to the first addr in addrList
    SensorData sd = deserializeStringToSensorData(str);
    // check if the MACaddr is self
    if (strcmp(sd.MACaddr, selfAddr) != 0) {
      dataReceived.removeFromFirst();
      // Serial.println("Data Message received but self is the receiver, remove from dataReceived queue");
      return;
    }
    // forward data msg
    dataToSend.addToLast(serializeSensorDataToString(&sd));

    // reply data msg
    sdr.randomNumber = sd.randomNumber;
    dataToSend.addToFirst(serializeSDRToString());

    dataReceived.removeFromFirst();
    // Serial.println("Self not isolated, add to dataToSend queue and remove from dataReceived queue");

  } else if (msgType == String(DATA_REPLY_MESSAGE)) {
    // Serial.println("Message is DATA_REPLY_MESSAGE");
    SensorDataReply received = deserializeStringToSDR(str);
    // loop through dataSending to find the corresponding data
    // if found, remove from dataSending
    if (dataSending.isEmpty() == false) {
      bool state = dataSending.removeIfMatches([received](String data) {
        SensorData sd = deserializeStringToSensorData(&data);
        return sd.randomNumber == received.randomNumber;
      });

      if (state == true) {
        replyTimerFlag = false;
        retry_fail_count = 0;
        // Serial.println("Found record, clear replyFlag and remove from dataSending queue");
      }
    }
    dataReceived.removeFromFirst();
    // Serial.println("DATA_REPLY_MESSAGE processed and removed from dataReceived queue");
  } else {
    dataReceived.removeFromFirst();
    // Serial.println("Received message cannot be recognised: ");
    // Serial.println(*str);
  }
}  // processStringReceived

void loopLoRa() {
  // regularly get the sensor data to send
  unsigned long timeNow = millis();
  static unsigned long sensorTimer = 0;
  if (timeNow - sensorTimer >= SENSOR_DATA_INTERVAL) {
    // Serial.println("Attempt to get sensor data");
    sensorTimer = timeNow;
    if (getSensorReading() == true) {
      sensorData.randomNumber = getRandomInt(1000, 9999);
      strncpy(sensorData.SMACaddr, selfAddr, MAX_MAC_LENGTH);
      dataToSend.addToLast(serializeSensorDataToString(&sensorData));
      // Serial.println("Add sensor data to dataToSend queue");
    }
  }

  if (rxFlag == true) {
    String receivedMsg;

    int state = radio.readData(receivedMsg);

    if (state == RADIOLIB_ERR_NONE) {
      // if packet is of type DataReplyMessage or DiscoveryReplyMessage, add to first of dataReceived
      if (receivedMsg.charAt(0) == '1' || receivedMsg.charAt(0) == '3') {
        dataReceived.addToFirst(receivedMsg);
        // Serial.print("Putting msg at the HEAD of dataReceived queue: ");
        // Serial.println(receivedMsg);
      } else {
        dataReceived.addToLast(receivedMsg);
        // Serial.print("Putting msg at the TAIL of dataReceived queue: ");
        // Serial.println(receivedMsg);
      }

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      // Serial.println(F("[SX1280] CRC error!"));

    } else {
      // some other error occurred
      // Serial.print(F("[SX1280] Failed, code "));
      // Serial.println(state);
    }
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      // Serial.println(F("success!"));
    } else {
      // Serial.print(F("failed, code "));
      // Serial.println(state);
      while (true)
        ;
    }
    rxFlag = false;
    enableInterrupt = true;
    return;
  }

  if (txDone == true) {
    txDone = false;
    int state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      // Serial.println(F("success!"));
      return;
    } else {
      // Serial.print(F("failed, code "));
      // Serial.println(state);
      while (true)
        ;
    }
  }

  // if timer is set, check if it's time
  if (replyTimerFlag == true) {
    if (timeNow - replyTimer >= WAITING_THRESHOLD) {
      retry_fail_count++;
      replyTimerFlag = false;

      dataSending.removeIfMatches([](String data) {
        return data.charAt(0) == '0';
      });

      while (dataSending.isEmpty() == false) {
        dataToSend.addToLast(dataSending.head->data);
        dataSending.removeFromFirst();
      }

      if (retry_fail_count >= MAX_RETRY) {
        addrList.removeFromFirst();
        retry_fail_count = 0;
      }
    }
  }

  // check if addrList is empty, if it is, turn off isolated mode
  if (addrList.isEmpty() == true) {
    isolated = true;
    selfLevel = 2147483647;
    if (replyTimerFlag == false) {
      dataToSend.addToFirst(serializeDMToString());
      // Serial.println("Add Discovery Message to the HEAD of dataToSend queue");
    }
    // Serial.println("addrList is empty, set isolated to TRUE");
    if (u8g2) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, "Isolated");
      u8g2->drawStr(0, 24, ("received: " + String(dataReceived.count)).c_str());
      u8g2->drawStr(0, 36, ("toSend: " + String(dataToSend.count)).c_str());
      u8g2->drawStr(0, 48, ("sending: " + String(dataSending.count)).c_str());
      u8g2->drawStr(0, 60, ("level: " + String(selfLevel)).c_str());
      u8g2->sendBuffer();
    }
  } else if (addrList.isEmpty() == false) {
    isolated = false;
    // Serial.println("addrList is not empty, set isolation to FALSE");
    if (u8g2) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, ("Connected: " + String(addrList.count)).c_str());
      u8g2->drawStr(0, 24, ("received: " + String(dataReceived.count)).c_str());
      u8g2->drawStr(0, 36, ("toSend: " + String(dataToSend.count)).c_str());
      u8g2->drawStr(0, 48, ("sending: " + String(dataSending.count)).c_str());
      u8g2->drawStr(0, 60, ("level: " + String(selfLevel)).c_str());
      u8g2->sendBuffer();
    }
  }

  // if is not isolated, attempt to send data in queue
  if (dataToSend.isEmpty() == false) {
    String data = dataToSend.head->data;
    transmitData(&data);
  }

  // regardless, process the data received in queue
  if (dataReceived.isEmpty() == false) {
    String data = dataReceived.head->data;
    processStringReceived(&data);
  }
  delay(2);
}  // loopLoRa
