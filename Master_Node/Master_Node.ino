// MASTER
#include <WiFi.h>

//ESPNOW
#include <esp_now.h>
#include <PubSubClient.h>

// LoRa
#include <RadioLib.h>
#include "config/boards.h"

#include "lib/message.h"
#include "lib/lora_impl.h"
#include "lib/esp_now_impl.h"

void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);
  Serial.println("ESP-NOW Broadcast Demo");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  WiFi.disconnect();
}

void initESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    // esp_now_register_send_cb(sentCallback);
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }
}

void initLoRa() {
  initBoard();
  setupLoRa();
}


void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Set up WiFi
  setupWiFi();

  // Initialize ESP-NOW
  initESPNow();

  // Initialize LoRa
  initLoRa();

  prev_time = millis();
  Serial.println("Finish Setup");
}




void loop() {
  loRaLoop();
}