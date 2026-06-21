#include <Arduino_GFX_Library.h>

Arduino_DataBus *bus = new Arduino_HWSPI(40, 41, 21, 47, -1);
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, 45, 0, true, 240, 240);

#define TFT_BL 42

const int PINS[] = {10, 11, 12, 13, 14};
const int N = 5;

void setup() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(30, 10);
    gfx->print("Button Test");
    gfx->setTextSize(1);
    gfx->setTextColor(0x4208);
    gfx->setCursor(10, 35);
    gfx->print("1=not pressed  0=pressed");

    for (int i = 0; i < N; i++)
        pinMode(PINS[i], INPUT_PULLUP);
}

void loop() {
    for (int i = 0; i < N; i++) {
        int val = digitalRead(PINS[i]);
        gfx->fillRect(0, 60 + i * 36, 240, 32, RGB565_BLACK);
        gfx->setTextSize(2);
        gfx->setTextColor(val == LOW ? RGB565_RED : RGB565_GREEN);
        gfx->setCursor(20, 66 + i * 36);
        gfx->printf("GPIO %2d : %s", PINS[i], val == LOW ? "PRESSED " : "        ");
    }
    delay(80);
}
