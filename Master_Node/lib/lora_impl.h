/** LoRa Implementation
 *  This file contains the implementation of the LoRa communication protocol
 *  for the T3 S3 V1.1 with PA module.
 */

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
    Serial.printf("%s,%.2f,%.2f,%.2f,%s\n", receivedData.SMACaddr, receivedData.c02Data, receivedData.temperatureData, receivedData.humidityData, "lora");

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


/*------------------------------------------------------------------*/
