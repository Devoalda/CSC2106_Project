// Client

#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>


// Struc for transmiting sensor data
struct SensorData {
  uint8_t rootNodeAddress;
  float c02Data;
  float temperatureData;
  float thirdValue;
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

// init global variables
SensorData sensorData;
Handshake msg;
esp_now_peer_info_t peerInfo = {};
esp_now_peer_num_t peer_num;
uint8_t isConnectedToMaster = 0;
uint8_t numberOfHopsToMaster = 0;


// Maximum number of nodes in the network
#define MAX_NODES 10


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
  Serial.println("Sending....");
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


void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
// Called when data is received
{
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // when sensor data received
  if (dataLen == sizeof(SensorData)) {
    Serial.println("Sensor Data received");
    // Message is sensor data from peer nodes
    SensorData receivedData;
    memcpy(&receivedData, data, sizeof(SensorData));

    // Print the received sensor data
    Serial.printf("Received sensor data from: %s\n", macStr);
    Serial.printf("Root Node Address: %d\n", receivedData.rootNodeAddress);
    Serial.printf("CO2 Data: %.2f\n", receivedData.c02Data);
    Serial.printf("Temperature Data: %.2f\n", receivedData.temperatureData);
    Serial.printf("Third Value: %.2f\n", receivedData.thirdValue);

    // Send the message to all peers in the peer list
    sendToAllPeers(receivedData);
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
      replyMsg.numberOfHopsToMaster = numberOfHopsToMaster;

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
        // Set current node status
        isConnectedToMaster = 1;
        // Set current hop count to master
        numberOfHopsToMaster = receivedMsg.numberOfHopsToMaster + 1;
        Serial.printf("Hop Count: %d", numberOfHopsToMaster);
        // Add this node to peer list
        addPeerToPeerList(macAddr);
      } else {
        Serial.printf("Node %s is NOT CONNECTED to master\n", macStr);
      }
    }


  } else {
    Serial.println("Received data length does not match expected formats");
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
float getRandomFloat(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  // char buffer[10];                     // Adjust the size as needed
  // dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  // String floatString = String(buffer);

  return randomFloat;
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
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Broadcast Demo");

  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Disconnect from WiFi
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
    // esp_now_register_send_cb(onDataSent);
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }

  // esp_wifi_set_promiscuous(true);
  // esp_wifi_set_channel(13, WIFI_SECOND_CHAN_NONE);
  // esp_wifi_set_promiscuous(false);
}


void loop() {
  sensorData.rootNodeAddress = 0;
  sensorData.c02Data = getRandomFloat(100.0, 1000.0);
  sensorData.temperatureData = getRandomFloat(0.0, 40.0);
  sensorData.thirdValue = getRandomFloat(90.0, 1030.0);

  esp_now_get_peer_num(&peer_num);
  Serial.printf("Peer num: %d\n", peer_num);

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
  if (peer_num.total_num == 0) {
    Serial.println("Commencing route discovery");
    // set msg header to request
    msg.requestType = 0;
    broadcast(msg);
  }
  // else send data to all in peer list
  else {
    Serial.println("Sending data to all peers");
    sendToAllPeers(sensorData);
  }


  // Delay for 5 seconds
  delay(5000);
}