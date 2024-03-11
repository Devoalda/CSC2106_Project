// MASTER

#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Define the maximum length of the MAC address
const int MAX_MAC_LENGTH = 18;  // For example, a MAC address is usually 17 characters long (including colons) plus 1 for null terminator

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
};

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
}


void loop() {
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