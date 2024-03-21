#include "LoraCommunication.h"

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
volatile uint8_t retry_fail_count = 0;
volatile unsigned long replyTimer;
volatile bool replyTimerFlag = false;
volatile bool isolated = true;
volatile bool txStart = false;
volatile bool txDone = false;
volatile bool enableInterrupt = true;
volatile bool rxFlag = false;
volatile int selfLevel = 2147483647;
LoraSensorData loraSensorData = {.requestType = DATA_MESSAGE};
SensorDataReply sensorDataReply = {.requestType = DATA_REPLY_MESSAGE};
DiscoveryMessage discoveryMessage = {.requestType = DISCOVERY_MESSAGE};
DiscoveryReplyMessage discoveryReplyMessage = {.requestType = DISCOVERY_REPLY_MESSAGE};


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

uint16_t error;
char errorMessage[256];
uint16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
bool isDataReady = false;

bool getSensorReading() {
  error = scd4x.getDataReadyFlag(isDataReady);
  if (error) {
    Serial.print("Error trying to execute getDataReadyFlag(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    // If error, use random data
    loraSensorData.c02Data = getRandomFloat(100.0, 1000.0);
    loraSensorData.temperatureData = getRandomFloat(0.0, 40.0);
    loraSensorData.humidityData = getRandomFloat(90.0, 1030.0);
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
      loraSensorData.c02Data = getRandomFloat(100.0, 1000.0);
      loraSensorData.temperatureData = getRandomFloat(0.0, 40.0);
      loraSensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else if (co2 == 0) {
      Serial.println("Invalid sample detected, skipping.");
      // If invalid sample, use random data
      loraSensorData.c02Data = getRandomFloat(100.0, 1000.0);
      loraSensorData.temperatureData = getRandomFloat(0.0, 40.0);
      loraSensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else {
      // Assign sensor data to loraSensorData structure
      loraSensorData.c02Data = co2;
      loraSensorData.temperatureData = temperature;
      loraSensorData.humidityData = humidity;
    }
  }
  return true;
}

String serializeSDRToString() {
  String result;

  result.concat(sensorDataReply.requestType);
  result += ",";
  result.concat(sensorDataReply.randomNumber);

  return result;
}

SensorDataReply deserializeStringToSDR(String* serialized) {
  SensorDataReply sensorDataReply;
  int firstCommaIndex = serialized->indexOf(',');

  // Convert and assign requestType
  sensorDataReply.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Convert and assign randomNumber
  sensorDataReply.randomNumber = serialized->substring(firstCommaIndex + 1).toInt();

  return sensorDataReply;
}

String serializeDMToString() {
  String result;

  result.concat(discoveryMessage.requestType);
  result += ",";  // add for substring to work

  return result;
}

String serializeDRMToString() {
  String result;

  result.concat(discoveryReplyMessage.requestType);
  result += ",";
  result.concat(discoveryReplyMessage.level);
  result += ",";
  result += MACaddrG;

  return result;
}

DiscoveryReplyMessage deserializeStringToDRM(String* serialized) {
  DiscoveryReplyMessage discoveryReplyMessage;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);

  // Convert and assign requestType
  discoveryReplyMessage.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Convert and assign level
  discoveryReplyMessage.level = serialized->substring(firstCommaIndex + 1, secondCommaIndex).toInt();

  // Assign MACaddr
  String macAddrString = serialized->substring(secondCommaIndex + 1);
  macAddrString.toCharArray(discoveryReplyMessage.MACaddr, MAX_MAC_LENGTH);

  return discoveryReplyMessage;
}

String serializeSensorDataToString(LoraSensorData* data) {
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

LoraSensorData deserializeStringToSensorData(String* serialized) {
  LoraSensorData loraSensorData;
  int firstCommaIndex = serialized->indexOf(',');
  int secondCommaIndex = serialized->indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = serialized->indexOf(',', secondCommaIndex + 1);
  int fourthCommaIndex = serialized->indexOf(',', thirdCommaIndex + 1);
  int fifthCommaIndex = serialized->indexOf(',', fourthCommaIndex + 1);
  int sixthCommaIndex = serialized->indexOf(',', fifthCommaIndex + 1);

  // Convert and assign requestType
  loraSensorData.requestType = (uint8_t)serialized->substring(0, firstCommaIndex).toInt();

  // Assign MACaddr (receiver)
  String macAddrString = serialized->substring(firstCommaIndex + 1, secondCommaIndex);
  macAddrString.toCharArray(loraSensorData.MACaddr, MAX_MAC_LENGTH);

  // sender's mac addr
  String sMacAddrString = serialized->substring(secondCommaIndex + 1, thirdCommaIndex);
  sMacAddrString.toCharArray(loraSensorData.SMACaddr, MAX_MAC_LENGTH);

  // Convert and assign c02Data
  loraSensorData.c02Data = serialized->substring(thirdCommaIndex + 1, fourthCommaIndex).toFloat();

  // Convert and assign temperatureData
  loraSensorData.temperatureData = serialized->substring(fourthCommaIndex + 1, fifthCommaIndex).toFloat();

  // Convert and assign humidityData
  loraSensorData.humidityData = serialized->substring(fifthCommaIndex + 1, sixthCommaIndex).toFloat();

  loraSensorData.randomNumber = serialized->substring(sixthCommaIndex + 1).toFloat();

  return loraSensorData;
}  // deserializeStringToSensorData

void transmitData(String* data) {
  // if its isolated and not sending the discovery message
  if (isolated == true && data->charAt(0) != String(DISCOVERY_MESSAGE).charAt(0)) {
    return;
  }

  // if waiting for reply, and data is Data message, do nothing
  if (replyTimerFlag == true && data->charAt(0) == String(DATA_MESSAGE).charAt(0)) {
    return;
  }

  // if message is DataMessage, add first addr
  if (data->charAt(0) == String(DATA_MESSAGE).charAt(0)) {
    const char* addrChar = addrList.head->data.c_str();
    LoraSensorData sd = deserializeStringToSensorData(data);
    strncpy(sd.MACaddr, addrChar, MAX_MAC_LENGTH);
    *data = serializeSensorDataToString(&sd);
  }

  // switch to transmit mode and send data
  int state = radio.startTransmit(*data);

  if (state == RADIOLIB_ERR_NONE) {
    txStart = true;
    // if data is of type SensorData or DiscoveryMessage, add to dataSending
    if (data->charAt(0) == String(DATA_MESSAGE).charAt(0) || data->charAt(0) == String(DISCOVERY_MESSAGE).charAt(0)) {
      dataSending.addToLast(*data);

      // set replyTimer to current time if not set
      if (replyTimerFlag == false) {
        replyTimer = millis();
        replyTimerFlag = true;
      }
    }
    // successfully sent
    dataToSend.removeFromFirst();
  }
}  // transmitData

void processStringReceived(String* str) {
  // check msg type by spliting the str and check the first int
  int commaIndex = str->indexOf(',');              // Find the index of the first comma
  String msgType = str->substring(0, commaIndex);  // Extract the substring from the beginning to the comma
  if (msgType == String(DISCOVERY_MESSAGE)) {
    // reply with DISCOVERY_REPLY_MESSAGE if self isolated is false, else do nothing
    if (isolated == false) {
      discoveryReplyMessage.level = selfLevel;
      dataToSend.addToFirst(serializeDRMToString());
    }
    dataReceived.removeFromFirst();

  } else if (msgType == String(DISCOVERY_REPLY_MESSAGE)) {
    // Serial.println("Message is DISCOVERY_REPLY_MESSAGE");
    // if self isolated is true, update addrList and level
    DiscoveryReplyMessage received = deserializeStringToDRM(str);

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
      }
      addrList.addToLast(String(received.MACaddr));
      isolated = false;
      replyTimerFlag = false;

      dataSending.removeIfMatches([](String data) {
        return data.charAt(0) == '0';
      });
    }

    dataReceived.removeFromFirst();

  } else if (msgType == String(DATA_MESSAGE)) {
    // if not isolated, forward the data message to the first addr in addrList
    LoraSensorData sd = deserializeStringToSensorData(str);
    // check if the MACaddr is self
    if (strcmp(sd.MACaddr, MACaddrG) != 0) {
      dataReceived.removeFromFirst();
      return;
    }
    // forward data msg
    dataToSend.addToLast(serializeSensorDataToString(&sd));

    // reply data msg
    sensorDataReply.randomNumber = sd.randomNumber;
    dataToSend.addToFirst(serializeSDRToString());

    dataReceived.removeFromFirst();

  } else if (msgType == String(DATA_REPLY_MESSAGE)) {
    SensorDataReply received = deserializeStringToSDR(str);
    // loop through dataSending to find the corresponding data
    // if found, remove from dataSending
    if (dataSending.isEmpty() == false) {
      bool state = dataSending.removeIfMatches([received](String data) {
        LoraSensorData sd = deserializeStringToSensorData(&data);
        return sd.randomNumber == received.randomNumber;
      });

      if (state == true) {
        replyTimerFlag = false;
        retry_fail_count = 0;
      }
    }
    dataReceived.removeFromFirst();
  } else {
    dataReceived.removeFromFirst();
  }
}  // processStringReceived

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
  }
}  // setFlag

void loraSetup() {
    Serial.print(F("LoRa Initializing ... "));

    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    Wire.begin(18, 17);

    int state = radio.begin();
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
}

void loraLoop() {
  Serial.println("loraLoop");
  // regularly get the sensor data to send
  unsigned long timeNow = millis();
  static unsigned long sensorTimer = 0;
  if (timeNow - sensorTimer >= SENSOR_DATA_INTERVAL) {
    sensorTimer = timeNow;
    if (getSensorReading() == true) {
      loraSensorData.randomNumber = getRandomInt(1000, 9999);
      strncpy(loraSensorData.SMACaddr, MACaddrG, MAX_MAC_LENGTH);
      dataToSend.addToLast(serializeSensorDataToString(&loraSensorData));
    }
  }

  if (rxFlag == true) {
    Serial.println("rxFlag");
    String receivedMsg;
    int state = radio.readData(receivedMsg);
    if (state == RADIOLIB_ERR_NONE) {
      // if packet is of type DataReplyMessage or DiscoveryReplyMessage, add to first of dataReceived
      if (receivedMsg.charAt(0) == String(DATA_REPLY_MESSAGE).charAt(0) || receivedMsg.charAt(0) == String(DISCOVERY_REPLY_MESSAGE).charAt(0)) {
        dataReceived.addToFirst(receivedMsg);
      } else {
        dataReceived.addToLast(receivedMsg);
      }

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
       // packet was received, but is malformed
       Serial.println(F("[SX1280] CRC error!"));

    } else {
       // some other error occurred
       Serial.print(F("[SX1280] Failed, code "));
       Serial.println(state);
    }
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      while (true)
        ;
    }
    rxFlag = false;
    enableInterrupt = true;
    return;
  }

  if (txDone == true) {
    Serial.println("txDone");
    txDone = false;
    int state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      while (true)
        ;
    }
  }

  // if timer is set, check if it's time
  if (replyTimerFlag == true) {
    if (timeNow - replyTimer >= WAITING_THRESHOLD) {
      Serial.println("replyTimerFlag");
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
    }
  } else if (addrList.isEmpty() == false) {
    isolated = false;
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
}  // loop