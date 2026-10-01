#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "hardware/gpio.h"

// Display Pinnen
#define TFT_CS   17
#define TFT_DC   20
#define TFT_RST  21
#define TFT_SCK  18
#define TFT_MOSI 19

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define BYTES_PER_LINE (SCREEN_WIDTH * 2) // 640 bytes

// Scanline buffer: 320 uint16_t waarden = 640 bytes
uint16_t scanlineBuffer[SCREEN_WIDTH];
uint8_t* payloadBuffer = (uint8_t*)scanlineBuffer;

enum RxState {
  WAIT_SYNC1,
  WAIT_SYNC2,
  WAIT_SYNC3,
  READ_LINE,
  READ_LEN_HIGH,
  READ_LEN_LOW,
  READ_PAYLOAD,
  READ_CRC
};

RxState currentState = WAIT_SYNC1;
uint8_t targetLine = 0;
uint16_t expectedLength = 0;
uint16_t payloadIndex = 0;
uint8_t calculatedChecksum = 0;

void setup() {
  Serial.begin(115200);

  // Hardware UART0 op GP1 @ 19200 baud met signaalinversie voor de fotodiode
  Serial1.setRX(1);
  Serial1.setTX(0);
  Serial1.begin(19200);
  gpio_set_inover(1, GPIO_OVERRIDE_INVERT);

  // SPI en Display setup
  SPI.setSCK(TFT_SCK);
  SPI.setTX(TFT_MOSI);
  SPI.begin();

  tft.begin(24000000);
  tft.setRotation(3); // Landscape (320x240)
  tft.fillScreen(ILI9341_BLACK);

  Serial.println("Pico klaar voor Full-Color RGB565 LiFi...");
}

void loop() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();

    switch (currentState) {
      case WAIT_SYNC1:
        if (b == 0x5A) currentState = WAIT_SYNC2;
        break;

      case WAIT_SYNC2:
        if (b == 0xA5) currentState = WAIT_SYNC3;
        else if (b != 0x5A) currentState = WAIT_SYNC1;
        break;

      case WAIT_SYNC3:
        if (b == 0xFF) currentState = READ_LINE;
        else currentState = WAIT_SYNC1;
        break;

      case READ_LINE:
        targetLine = b;
        calculatedChecksum = b;
        currentState = READ_LEN_HIGH;
        break;

      case READ_LEN_HIGH:
        expectedLength = ((uint16_t)b) << 8;
        calculatedChecksum ^= b;
        currentState = READ_LEN_LOW;
        break;

      case READ_LEN_LOW:
        expectedLength |= b;
        calculatedChecksum ^= b;
        if (expectedLength == BYTES_PER_LINE) {
          payloadIndex = 0;
          currentState = READ_PAYLOAD;
        } else {
          currentState = WAIT_SYNC1;
        }
        break;

      case READ_PAYLOAD:
        payloadBuffer[payloadIndex++] = b;
        calculatedChecksum ^= b;
        if (payloadIndex >= expectedLength) {
          currentState = READ_CRC;
        }
        break;

      case READ_CRC:
        if (b == calculatedChecksum) {
          if (targetLine < SCREEN_HEIGHT) {
            tft.drawRGBBitmap(0, targetLine, scanlineBuffer, SCREEN_WIDTH, 1);
          }
        } else {
          Serial.printf("[CRC FOUT] Lijn %d\n", targetLine);
        }
        currentState = WAIT_SYNC1;
        break;
    }
  }
}