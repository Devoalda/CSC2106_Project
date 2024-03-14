#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Ticker.h>
#include "utilities.h"

SPIClass SDSPI(HSPI);
#include <U8g2lib.h>
#define DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C

DISPLAY_MODEL *u8g2 = nullptr;

Ticker ledTicker;

void initBoard() {
  Serial.begin(115200);
  Serial.println("initBoard");
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, HIGH);
  delay(20);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  delay(20);

  pinMode(BOARD_LED, OUTPUT);
  ledTicker.attach_ms(500, []() {
    static bool level;
    digitalWrite(BOARD_LED, level);
    level = !level;
  });

  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    Serial.println("Started OLED");
    u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
    u8g2->begin();
    u8g2->clearBuffer();
    u8g2->setFlipMode(0);
    u8g2->setFontMode(1);  // Transparent
    u8g2->setDrawColor(1);
    u8g2->setFontDirection(0);
    u8g2->firstPage();
    do {
      u8g2->setFont(u8g2_font_inb19_mr);
      u8g2->drawStr(0, 30, "LilyGo");
      u8g2->drawHLine(2, 35, 47);
      u8g2->drawHLine(3, 36, 47);
      u8g2->drawVLine(45, 32, 12);
      u8g2->drawVLine(46, 33, 12);
      u8g2->setFont(u8g2_font_inb19_mf);
      u8g2->drawStr(58, 60, "LoRa");
    } while (u8g2->nextPage());
    u8g2->sendBuffer();
    u8g2->setFont(u8g2_font_fur11_tf);
    delay(3000);
  }
}
