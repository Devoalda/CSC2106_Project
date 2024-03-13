// MASTER
#include <WiFi.h>
// LoRa
#include <RadioLib.h>
#include "boards.h"

#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

#define DISCOVERY_DURATION 5000

bool setup_complete = false;

// flag to indicate that a sensor data packet to be sent, for identifying the callback is triggered by transmit
volatile bool txFlag = false;

// flag to indicate to call radio.startReceive()
volatile bool rxFlag = false;

volatile bool transmitted = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

// Define the maximum length of the MAC address
const int MAX_MAC_LENGTH = 18;
char selfAddr[MAX_MAC_LENGTH];

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;

DiscoveryMessage dm = { DISCOVERY_MESSAGE };

typedef struct SensorData {
  // uint8_t rootNodeAddress;
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  float c02Data;
  float temperatureData;
  float humidityData;
  uint8_t randomNumber;
} SensorData;

SensorData sensorData = {
  .requestType = 2
};

unsigned long curr_time;
unsigned long prev_time;

template<typename T>
class Queue {
public:
  struct Node {
    T data;
    Node* next;
  };

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
      return;
    }

    Node* last = head;
    while (last->next != NULL) {
      last = last->next;
    }

    last->next = newNode;
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
  }

  void removeFromFirst() {
    if (head == NULL) {
      // The list is already empty
      return;
    }
    Node* tempNode = head;
    head = head->next;
    delete tempNode;
  }

  bool isEmpty() {
    return head == NULL;
  }

  void clear() {
    while (head != NULL) {
      removeFromFirst();
    }
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
        return true;
      }
      prev = current;
      current = current->next;
    }
    return false;
  }
};

Queue<String> dataReceived;

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
  result += ",";
  result.concat(sensorData.randomNumber);

  return result;
}

SensorData deserializeStringToSensorData(String* serialized) {
  SensorData sensorData;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = serialized->indexOf(',', secondCommaIndex + 1);
  int fourthCommaIndex = serialized->indexOf(',', thirdCommaIndex + 1);
  int fifthCommaIndex = serialized->indexOf(',', fourthCommaIndex + 1);

  // Convert and assign requestType
  sensorData.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized->substring(firstCommaIndex + 1, secondCommaIndex);
  macAddrString.toCharArray(sensorData.MACaddr, MAX_MAC_LENGTH);

  // Convert and assign c02Data
  sensorData.c02Data = serialized->substring(secondCommaIndex + 1, thirdCommaIndex).toFloat();

  // Convert and assign temperatureData
  sensorData.temperatureData = serialized->substring(thirdCommaIndex + 1, fourthCommaIndex).toFloat();

  // Convert and assign humidityData
  sensorData.humidityData = serialized->substring(fourthCommaIndex + 1).toFloat();

  sensorData.randomNumber = serialized->substring(fifthCommaIndex + 1).toFloat();

  return sensorData;
}  // deserializeStringToSensorData

// this function is called when a complete packet
// is received or sent by the module
void setFlag(void) {
  // if not transmitting, means triggered by packet receiving
  // if multiple packet is received at the same time when the later one
  // is received before the prior one is read and stored, the later one
  // will not be captured!
  if (txFlag == false && enableInterrupt == true) {
    enableInterrupt = false;
    rxFlag = true;
  } else if (txFlag == true) {
    // end of sending
    txFlag = false;
    transmitted = true;
    Serial.println("Message sending completed");
  } else {
    return;
  }
}  // setFlag

// Function to parse a String MAC address to uint8_t array
void parseMacAddress(String macAddress, uint8_t *macAddressBytes) {
  sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &macAddressBytes[0], &macAddressBytes[1], &macAddressBytes[2],
         &macAddressBytes[3], &macAddressBytes[4], &macAddressBytes[5]);
}

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

String serializeDMToString() {
  String result;

  result.concat(dm.requestType);
  result += ",";  // add for substring to work

  return result;
}

void processStringReceived(String* str) {
  Serial.println(F("Master received packet!"));

  // print data of the packet
  // Serial.print(F("Data:\t\t"));
  // Serial.println(*str);
  // check msg type by spliting the str and check the first int
  int commaIndex = str->indexOf(',');              // Find the index of the first comma
  String msgType = str->substring(0, commaIndex);  // Extract the substring from the beginning to the comma
  if (msgType == String(DATA_MESSAGE)) {
    // print data is dictated format in terminal
    SensorData receivedData;
    receivedData = deserializeStringToSensorData(str);

    Serial.printf("%s,%.2f,%.2f,%.2f\n", receivedData.MACaddr, receivedData.c02Data, receivedData.temperatureData, receivedData.humidityData);

    dataReceived.removeFromFirst();
  }

}  // processStringReceived

float getRandomFloat(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  return randomFloat;
}

void transmitData (String data) {
  // check if the previous transmission finished
    if (txFlag) {
        // disable the interrupt service routine while
        // processing the data
        enableInterrupt = false;

        // reset flag
        txFlag = false;

        if (transmissionState == RADIOLIB_ERR_NONE) {
            // packet was successfully sent
            Serial.println(F("transmission finished!"));

            // NOTE: when using interrupt-driven transmit method,
            //       it is not possible to automatically measure
            //       transmission data rate using getDataRate()

        } else {
            Serial.print(F("failed, code "));
            Serial.println(transmissionState);
        }

        // wait a second before transmitting again
        delay(2);

        // send another one
        Serial.println(F("[SX1280] Sending another packet ... "));

        transmissionState = radio.startTransmit(data);
        // transmissionState = radio.startTransmit("Hello Word!");

        // we're ready to send more packets,
        // enable interrupt service routine
        enableInterrupt = true;
    }
}

void setupLoRa() {
  // initialize SX1280 with default settings
  Serial.print(F("LoRa Initializing ... "));
  int state = radio.begin();

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

  // start transmitting the first packet
    Serial.print(F("[SX1280] Sending first packet ... "));

    // you can transmit C-string or Arduino string up to
    // 256 characters long
    transmissionState = radio.startTransmit("Hello World!");

}  // setupLoRa

void setup() {
  Serial.begin(115200);
  delay(1000);

  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  Serial.println("Obtaining the MAC address");
  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Print MAC address
  Serial.print("MAC Address: ");

  uint8_t macAddressBytes[6];
  parseMacAddress(WiFi.macAddress(), macAddressBytes);
  // Format the MAC address and store it in sensorData.MACaddr
  formatMacAddress(macAddressBytes, selfAddr, MAX_MAC_LENGTH);
  // formatMacAddress(macAddressBytes, drm.MACaddr, MAX_MAC_LENGTH);

  Serial.println(selfAddr);

  // Disconnect from WiFi
  WiFi.disconnect();

  setupLoRa();
  prev_time = millis();
  Serial.println("Finish Setup");
}

void loop() {
  //Loop Transmit
  curr_time = millis();
  Serial.println(curr_time-prev_time);
  //Start Discovery
  if (curr_time-prev_time < DISCOVERY_DURATION) {
    // Broadcast Discovery Packet
    Serial.println("Send Discovery");
    transmitData(serializeDMToString());

  } 
  
  // Switch to listen for 
  else {
    // testing receive sample data received
    strcpy(sensorData.MACaddr, "dc:54:75:e4:7e:75");
    sensorData.c02Data = getRandomFloat(100.0, 1000.0);
    sensorData.temperatureData = getRandomFloat(0.0, 40.0);
    sensorData.humidityData = getRandomFloat(90.0, 1030.0);
    String dataString = serializeSensorDataToString();
    dataReceived.addToLast(dataString);

    // regardless, process the data received in queue
    if (dataReceived.isEmpty() == false) {
      String data = dataReceived.head->data;
      processStringReceived(&data);
    }

  } 

  delay(1000);

}