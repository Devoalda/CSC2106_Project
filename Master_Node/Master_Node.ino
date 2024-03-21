// MASTER
#include <WiFi.h>

//ESPNOW
#include <esp_now.h>
#include <PubSubClient.h>

// LoRa
#include <RadioLib.h>
#include "boards.h"

// LoRa Defines
#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

// Self MAC Address
const int MAX_MAC_LENGTH = 18;
char selfAddr[MAX_MAC_LENGTH];

/*---------------------------ESPNOW Defines-----------------------*/

// Struc for transmitting sensor data
typedef struct SensorData {
  // uint8_t rootNodeAddress;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  float c02Data;
  float temperatureData;
  float humidityData;
} SensorData;


// Connection Request Struct
typedef struct Handshake {
  // 0 for request, 1 for reply
  uint8_t requestType;
  // 1 if connected to master, 0 otherwise
  uint8_t isConnectedToMaster;
  // number of hops to master
  uint8_t numberOfHopsToMaster;
} Handshake;

/*------------------------------------------------------------------*/

/*---------------------------LoRa Defines-----------------------*/
// Discovery Message
typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;

DiscoveryMessage dm = { DISCOVERY_MESSAGE };

// Struc for transmitting sensor data
typedef struct SensorDataLoRa {
  // uint8_t rootNodeAddress;
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  char SMACaddr[MAX_MAC_LENGTH]; // self mac
  float c02Data;
  float temperatureData;
  float humidityData;
  uint8_t randomNumber;
} SensorDataLoRa;

SensorDataLoRa sensorDataLoRa = {
  .requestType = 2
};

typedef struct SensorDataReply {
  uint8_t requestType;
  int randomNumber;  // from the data packet
} SensorDataReply;

SensorDataReply sdr = {
  .requestType = DATA_REPLY_MESSAGE
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
Queue<String> dataToSend;
/*------------------------------------------------------------------*/

/*----------------------LoRa Variables-----------------------------*/

// flag to indicate that a sensor data packet to be sent, for identifying the callback is triggered by transmit
volatile bool txFlag = false;

// flag to indicate to call radio.startReceive()
volatile bool rxFlag = false;

// flag to indicate the transmittion is completed, set to true in setFlag
volatile bool transmitted = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

// Define SX1280 Radio
SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);


String serializeDRMToString() {
  String result;

  result += "1";
  result += ",";
  result += "0";
  result += ",";
  result += selfAddr;

  return result;
}

String serializeSDRToString() {
  String result;

  result.concat(sdr.requestType);
  result += ",";
  result.concat(sdr.randomNumber);

  return result;
}

SensorDataLoRa deserializeStringToSensorData(String* serialized) {
  SensorDataLoRa sensorDataLoRa;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = serialized->indexOf(',', secondCommaIndex + 1);
  int fourthCommaIndex = serialized->indexOf(',', thirdCommaIndex + 1);
  int fifthCommaIndex = serialized->indexOf(',', fourthCommaIndex + 1);
  int sixthCommaIndex = serialized->indexOf(',', fifthCommaIndex + 1);

  // Convert and assign requestType
  sensorDataLoRa.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Assign MACaddr (receiver)
  String macAddrString = serialized->substring(firstCommaIndex + 1, secondCommaIndex);
  macAddrString.toCharArray(sensorDataLoRa.MACaddr, MAX_MAC_LENGTH);

  // sender's mac addr
  String sMacAddrString = serialized->substring(secondCommaIndex + 1, thirdCommaIndex);
  sMacAddrString.toCharArray(sensorDataLoRa.SMACaddr, MAX_MAC_LENGTH);

  // Convert and assign c02Data
  sensorDataLoRa.c02Data = serialized->substring(thirdCommaIndex + 1, fourthCommaIndex).toFloat();

  // Convert and assign temperatureData
  sensorDataLoRa.temperatureData = serialized->substring(fourthCommaIndex + 1, fifthCommaIndex).toFloat();

  // Convert and assign humidityData
  sensorDataLoRa.humidityData = serialized->substring(fifthCommaIndex + 1, sixthCommaIndex).toFloat();

  sensorDataLoRa.randomNumber = serialized->substring(sixthCommaIndex + 1).toFloat();

  return sensorDataLoRa;
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
    // Serial.println("Message sending completed");
  } else {
    return;
  }
}

String serializeDMToString() {
  String result;

  result.concat(dm.requestType);
  result += ",";  // add for substring to work

  return result;
}

void processStringReceived(String* str) {
  // Serial.println(F("Master received packet!"));

  // print data of the packet
  // Serial.print(F("Data:\t\t"));
  // Serial.println(*str);
  // check msg type by spliting the str and check the first int
  int commaIndex = str->indexOf(',');              // Find the index of the first comma
  String msgType = str->substring(0, commaIndex);  // Extract the substring from the beginning to the comma
  if (msgType == String(DATA_MESSAGE)) {
    // Serial.println("Message is DATA_MESSAGE");
    // print data is dictated format in terminal
    SensorDataLoRa receivedData = deserializeStringToSensorData(str);
    if (strcmp(receivedData.MACaddr, selfAddr) != 0) {
      dataReceived.removeFromFirst();
      // Serial.println("Data Message received but self is the receiver, remove from dataReceived queue");
      return;
    }

    // TODO: send to server?
    Serial.printf("%s,%.2f,%.2f,%.2f\n", receivedData.SMACaddr, receivedData.c02Data, receivedData.temperatureData, receivedData.humidityData);

    if (u8g2) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, (String(receivedData.SMACaddr)).c_str());
      u8g2->drawStr(0, 24, ("CO2: " + String(receivedData.c02Data)).c_str());
      u8g2->drawStr(0, 36, ("Temp: " + String(receivedData.temperatureData)).c_str());
      u8g2->drawStr(0, 48, ("Humidity: " + String(receivedData.humidityData)).c_str());
      u8g2->sendBuffer();
    }

    // reply data msg
    sdr.randomNumber = receivedData.randomNumber;
    dataToSend.addToLast(serializeSDRToString());

    dataReceived.removeFromFirst();
    // Serial.println("Add to dataToSend queue and remove from dataReceived queue");
  } else if (msgType == String(DISCOVERY_MESSAGE)) {
    // Serial.println("Message is DISCOVERY_MESSAGE");
    dataToSend.addToLast(serializeDRMToString());
    dataReceived.removeFromFirst();
    // Serial.println("Add discovery reply message to queue");
  } else {
    // remove
    dataReceived.removeFromFirst();
    // Serial.println("Received message cannot be recognised: ");
    // Serial.println(*str);
  }

}  // processStringReceived

void transmitData(String* data) {
  // Serial.println(F("Transmitting packet..."));
  // Serial.println(*data);

  // master node only sends Discovery reply or Data reply message
  int state = radio.startTransmit(*data);
  if (state == RADIOLIB_ERR_NONE) {
    txFlag = true;
    dataToSend.removeFromFirst();
    // Serial.println(F("Transmission init successful! remove from dataToSendQueue"));

  } else {
    // failed to send
    // Serial.print(F("Transmission failed, code "));
    // Serial.println(state);
  }
}

void setupLoRa() {
  // initialize SX1280 with default settings
  Serial.print(F("LoRa Initializing ... "));
  int state = radio.begin();

  Serial.println("Here");

  if (u8g2) {
    if (state != RADIOLIB_ERR_NONE) {
      u8g2->clearBuffer();
      u8g2->drawStr(0, 12, "LoRa Initializing: FAIL!");
      u8g2->sendBuffer();
    }
  }

  Serial.println("Here");

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }

  Serial.println("Here");

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

  // start transmitting the first packet
  Serial.print(F("[SX1280] Sending first packet ... "));

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  transmissionState = radio.startTransmit("Hello World!");

}  // setupLoRa

/*------------------------------------------------------------------*/

// init global vars
SensorData sensorData;
Handshake msg;
esp_now_peer_info_t peerInfo = {};
esp_now_peer_num_t peer_num;

// Maximum number of nodes in the network
#define MAX_NODES 10

// Global variable to track if the current node is connected to the master
uint8_t isConnectedToMaster = 1;

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

// Function to parse a String MAC address to uint8_t array
void parseMacAddress(String macAddress, uint8_t* macAddressBytes) 
{
  sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &macAddressBytes[0], &macAddressBytes[1], &macAddressBytes[2],
         &macAddressBytes[3], &macAddressBytes[4], &macAddressBytes[5]);
}

void addPeerToPeerList(const uint8_t *macAddr)
// Add the received MAC address into the peer list if it doesn't exist
{
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  Serial.printf("Received peer address: %s\n", macStr);

  memcpy(peerInfo.peer_addr, macAddr, 6);  // Copy MAC address
  peerInfo.channel = 0;                    // Use the default channel
  peerInfo.encrypt = false;                // No encryption for simplicity
  if (!esp_now_is_peer_exist(macAddr)) {
    esp_now_add_peer(&peerInfo);
    Serial.println("Added peer to the list");
  } else {
    Serial.println("Peer already exists in the list");
  }
}

void sendToAllPeers(const SensorData &sensorData)
// Send the message to each peer in the peer list
{
  esp_now_get_peer_num(&peer_num);
  // Serial.println("Sending....");
  for (int i = 0; i < peer_num.total_num; i++) {
    // Format the MAC address
    char macStr[18];
    formatMacAddress(peerInfo.peer_addr, macStr, 18);
    Serial.printf("Peer address to send: %s\n", macStr);
    if (esp_now_fetch_peer(1, &peerInfo) == ESP_OK) {
      esp_err_t result = esp_now_send(peerInfo.peer_addr, (const uint8_t *)&sensorData, sizeof(SensorData));

      // Print results to serial monitor
      if (result == ESP_OK) {
        Serial.printf("Forwarded message to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2],
                      peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
      } else {
        Serial.println("Error sending message to peer");
      }
    } else {
      Serial.println("Peer not found");
    }
  }
}

void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen) {
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // When sensor data received
  if (dataLen == sizeof(SensorData)) {
    // Message is sensor data from peer nodes
    SensorData receivedData;
    memcpy(&receivedData, data, sizeof(SensorData));

    // Print the received sensor data including MAC address
    Serial.printf("%s,%.2f,%.2f,%.2f\n", receivedData.MACaddr, receivedData.c02Data, receivedData.temperatureData, receivedData.humidityData);

  } else if (dataLen == sizeof(Handshake)) {
    Serial.println("Handshake received");
    // Message is a handshake message
    Handshake receivedMsg;
    memcpy(&receivedMsg, data, sizeof(Handshake));

    // handle requests
    if (receivedMsg.requestType == 0) {
      Serial.println("Request received");
      // Request type is 0, it's a request for connection status
      // Reply with the current connection status
      Handshake replyMsg;
      replyMsg.requestType = 1;
      replyMsg.isConnectedToMaster = isConnectedToMaster;
      replyMsg.numberOfHopsToMaster = 0;

      memcpy(peerInfo.peer_addr, macAddr, 6);
      if (!esp_now_is_peer_exist(macAddr)) {
        esp_now_add_peer(&peerInfo);
      }

      // send
      esp_err_t result = esp_now_send(macAddr, (const uint8_t *)&replyMsg, sizeof(Handshake));

      // Remove the broadcast address from the peer list
      esp_now_del_peer(macAddr);

      // Print results to serial monitor
      if (result == ESP_OK) {
        Serial.println("Reply message success");
      } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
        Serial.println("ESP-NOW not Init.");
      } else if (result == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
      } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
        Serial.println("Internal Error");
      } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("ESP_ERR_ESPNOW_NO_MEM");
      } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
        Serial.println("Peer not found.");
      } else {
        Serial.println("Unknown error");
      }
    }
    // handle replies
    else if (receivedMsg.requestType == 1) {
      Serial.println("Reply received");
      // Reply type is 1, it's a reply containing connection status
      if (receivedMsg.isConnectedToMaster) {
        Serial.printf("Node %s is CONNECTED to the master\n", macStr);
        // Add this node to peer list
        addPeerToPeerList(macAddr);
      } else {
        Serial.printf("Node %s is NOT CONNECTED to master\n", macStr);
      }
    }
  } else {
    Serial.println("Received data length does not match expected formats");
    Serial.print("Received data: ");
    for (int i = 0; i < dataLen; i++) {
      Serial.print(data[i], HEX);  // Print each byte of data in hexadecimal format
      Serial.print(" ");
    }
    Serial.println();  // Print a newline after printing all bytes
  }
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
// Called when data is sent
{
  // char macStr[18];
  // formatMacAddress(macAddr, macStr, 18);
  // Serial.print("Last Packet Sent to: ");
  // Serial.println(macStr);
  // Serial.print("Last Packet Send Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const Handshake &msg)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }

  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)&msg, sizeof(Handshake));

  // Remove the broadcast address from the peer list
  esp_now_del_peer(broadcastAddress);

  // Print results to serial monitor
  if (result == ESP_OK) {
    Serial.println("Broadcast message success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    Serial.println("ESP-NOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Unknown error");
  }
}

String getRandomFloatAsString(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  char buffer[10];                     // Adjust the size as needed
  dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  String floatString = String(buffer);

  return floatString;
}

void loRaLoop() {
  if (rxFlag == true) {
    String receivedMsg;

    int state = radio.readData(receivedMsg);

    if (state == RADIOLIB_ERR_NONE) {
      dataReceived.addToLast(receivedMsg);
      // Serial.print("Putting msg at the TAIL of dataReceived queue: ");
      // Serial.println(receivedMsg);
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("[SX1280] CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("[SX1280] Failed, code "));
      Serial.println(state);
    }
    // start listending again
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      // Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true)
        ;
    }
    rxFlag = false;
    enableInterrupt = true;
    return;
  }

  if (transmitted == true) {
    transmitted = false;
    int state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      // Serial.println(F("success!"));
      return;
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true)
        ;
    }
  }

  // regardless, process the data received in queue
  if (dataReceived.isEmpty() == false) {
    String data = dataReceived.head->data;
    processStringReceived(&data);
  }

  if (dataToSend.isEmpty() == false) {
    String data = dataToSend.head->data;
    transmitData(&data);
  }

  delay(2);
}

void setup() {

  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_AP_STA);
  Serial.println("ESP-NOW Broadcast Demo");

  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  uint8_t macAddressBytes[6];
  parseMacAddress(WiFi.macAddress(), macAddressBytes);
  // Format the MAC address and store it in sensorDataLoRa.MACaddr
  formatMacAddress(macAddressBytes, selfAddr, MAX_MAC_LENGTH);

  // WiFi.begin(ssid, password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println(WiFi.localIP());
  // WiFi.printDiag(Serial);

  // client.setServer(mqtt_server, 1883);

  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }

  // Initialise LoRa
  initBoard();
  setupLoRa();
  prev_time = millis();
  Serial.println("Finish Setup");
}


void loop() {
  loRaLoop();
  // sensorData.rootNodeAddress = 0;
  // sensorData.c02Data = 20.0;
  // sensorData.temperatureData = 20.0;
  // sensorData.thirdValue = 20.0;

  // esp_now_get_peer_num(&peer_num);
  // Serial.printf("Peer num: %d\n", peer_num);

  // Print peers in peer list
  // Serial.println("Peers in Peer List:");
  // for (int i = 0; i < peer_num.total_num; i++) {
  //   if (esp_now_fetch_peer(i, &peerInfo) == ESP_OK) {
  //     char macStr[18];
  //     formatMacAddress(peerInfo.peer_addr, macStr, 18);
  //     Serial.println(macStr);
  //   }
  // }


  // If there is no one in peerlist, do route discovery
  // if (peer_num.total_num == 0)
  // {
  //   Serial.println("Commencing route discovery");
  //   // set msg header to request
  //   msg.requestType = 0;
  //   broadcast(msg);
  // }
  // // else send data to all in peer list
  // else
  // {
  //   Serial.println("Sending data to all peers");
  //   sendToAllPeers(sensorData);
  // }
  // client.loop();
  // client.publish(SENSOR_DATA_TOPIC, "HELLO");

  // Delay for 5 seconds
  // delay(5000);
}