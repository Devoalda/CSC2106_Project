
#pragma once

#define LILYGO_T3_S3_V1_0    //SUPPORT

#define UNUSE_PIN                   (0)

#if defined(LILYGO_T3_S3_V1_0)

#define I2C_SDA                     18
#define I2C_SCL                     17
#define OLED_RST                    UNUSE_PIN

#define RADIO_SCLK_PIN              5
#define RADIO_MISO_PIN              3
#define RADIO_MOSI_PIN              6
#define RADIO_CS_PIN                7
#define RADIO_DIO1_PIN              9
#define RADIO_DIO2_PIN              33
#define RADIO_DIO3_PIN              34
#define RADIO_RST_PIN               8
#define RADIO_BUSY_PIN              36

#define RADIO_RX_PIN                21
#define RADIO_TX_PIN                10

#define SDCARD_MOSI                 11
#define SDCARD_MISO                 2
#define SDCARD_SCLK                 14
#define SDCARD_CS                   13

#define BOARD_LED                   37
#define LED_ON                      HIGH

#define BAT_ADC_PIN                 1
#define BUTTON_PIN                  0

// #define I2C_SDA                     46
// #define I2C_SCL                     45

// #define HAS_SDCARD
#define HAS_DISPLAY

#else
#error "For the first use, please define the board version and model in <utilities. h>"
#endif

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
