/*
   RadioLib SX128x Receive with Interrupts Example

   This example listens for LoRa transmissions and tries to
   receive them. Once a packet is received, an interrupt is
   triggered. To successfully receive data, the following
   settings have to be the same on both transmitter
   and receiver:
    - carrier frequency
    - bandwidth
    - spreading factor
    - coding rate
    - sync word

   Other modules from SX128x family can also be used.

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx128x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/


#include <RadioLib.h>
#include "boards.h"

SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

volatile bool transmittedFlag = false;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

volatile int count = 0;

typedef struct TestStruct {
  int IntVal;
  float FloatVal;
  String StringVal;
} TestStruct;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  if (!enableInterrupt) {
    return;
  }

  // we got a packet, set the flag
  receivedFlag = true;
}

String serializeTestStructToString(TestStruct* ts) {
  String result;

  // Concatenate integer and float values directly.
  // Use String constructor or concat() for conversion from numeric to String.
  result.concat(ts->IntVal);
  result += ",";  // Adding a separator for readability, can be omitted.
  result.concat(ts->FloatVal);
  result += ",";  // Adding a separator for readability, can be omitted.

  // StringVal is already a String, just concatenate.
  result += ts->StringVal;

  return result;
}

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
      u8g2->drawStr(0, 12, "Transmitting: OK!");
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

  // re-enable interrupt service routine
  enableInterrupt = true;
}

void setup() {
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  // initialize SX1280 with default settings
  Serial.print(F("[SX1280] Initializing ... "));
  int state = radio.begin();

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
}


void loop() {
  // testing
  static unsigned long lastTransmitTime = 0;
  const unsigned long transmitInterval = 10000;  // Interval to transmit, in milliseconds (e.g., 10000ms = 10 seconds)

  // Get the current time
  unsigned long currentTime = millis();

  // check if the flag is set
  if (receivedFlag) {
    // disable the interrupt service routine while
    // processing the data
    enableInterrupt = false;

    // reset flag
    receivedFlag = false;

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

      // print RSSI (Received Signal Strength Indicator)
      Serial.print(F("[SX1280] RSSI:\t\t"));
      Serial.print(radio.getRSSI());
      Serial.println(F(" dBm"));

      // print SNR (Signal-to-Noise Ratio)
      Serial.print(F("[SX1280] SNR:\t\t"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));

      if (u8g2) {
        u8g2->clearBuffer();
        char buf[256];
        u8g2->drawStr(0, 12, "Received OK!");
        snprintf(buf, sizeof(buf), "Data:%s", str);
        u8g2->drawStr(0, 26, buf);
        snprintf(buf, sizeof(buf), "RSSI:%.2f", radio.getRSSI());
        u8g2->drawStr(0, 40, buf);
        snprintf(buf, sizeof(buf), "SNR:%.2f", radio.getSNR());
        u8g2->drawStr(0, 54, buf);
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
    transmittedFlag = true;          // Set the flag to trigger transmission
  }

  if (transmittedFlag) {
    transmittedFlag = false;  // reset the flag

    TestStruct ts = {
      .IntVal = count++,
      .FloatVal = 4.2,
      .StringVal = "hello"
    };

    String msg = serializeTestStructToString(&ts);

    // call to transmit data
    transmitData(&msg);
  }
}
