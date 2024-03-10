#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Structure for the route table entry
struct RouteTableEntry {
  uint8_t destinationNodeAddress;
  uint8_t nextHopNodeAddress;
};

// Struc for transmiting sensor data
struct SensorData {
  uint8_t rootNodeAddress;
  float c02Data;
  float temperatureData;
  float thirdValue;
};

// Maximum number of nodes in the network
#define MAX_NODES 10

// Flag to determine master mesh node
#define MESH_MASTER false

// Route table to store next hop for each destination node
RouteTableEntry routeTable[MAX_NODES];

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void addPeerToPeerList(const uint8_t *macAddr)
// Add the received MAC address into the peer list if it doesn't exist
{
  esp_now_peer_info_t peerInfo = {};
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
  esp_now_peer_info_t peerInfo = {};
  esp_now_peer_num_t peer_num;
  esp_now_get_peer_num(&peer_num);

  for (int i = 0; i < peer_num.total_num; i++) {
    if (esp_now_fetch_peer(i, &peerInfo) == ESP_OK) {
      esp_err_t result = esp_now_send(peerInfo.peer_addr, (const uint8_t *)&sensorData, sizeof(SensorData));

      // Print results to serial monitor
      if (result == ESP_OK) {
        Serial.printf("Forwarded message to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2],
                      peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
      } else {
        Serial.println("Error sending message to peer");
      }
    }
  }
}

void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
// Called when data is received
{
  if (dataLen != sizeof(SensorData)) {
    Serial.println("Received data length does not match SensorData struct size");
    return;
  }
  Serial.println("Received data in correct format");

  // Parse the received SensorData struct
  SensorData receivedData;
  memcpy(&receivedData, data, sizeof(SensorData));

  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // Print the received sensor data
  Serial.printf("Received message from: %s\n", macStr);
  Serial.printf("Root Node Address: %d\n", receivedData.rootNodeAddress);
  Serial.printf("CO2 Data: %.2f\n", receivedData.c02Data);
  Serial.printf("Temperature Data: %.2f\n", receivedData.temperatureData);
  Serial.printf("Third Value: %.2f\n", receivedData.thirdValue);

  // Add the received MAC address into the peer list
  addPeerToPeerList(macAddr);

  // Send the message to all peers in the peer list
  sendToAllPeers(receivedData);
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


void broadcast(const SensorData &sensorData)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  esp_now_peer_info_t peerInfo = {};

  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)&sensorData, sizeof(SensorData));

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
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }

  // float min = 0.0;
  // float max = 100.0;

  //broadcast(getRandomFloatAsString(min, max));
}

void loop() {
  SensorData sensorData;
  sensorData.rootNodeAddress = 0;
  sensorData.c02Data = 20.0;
  sensorData.temperatureData = 20.0;
  sensorData.thirdValue = 20.0;

  // Broadcast sensor data
  broadcast(sensorData);

  // Delay for 5 seconds
  delay(5000);
}