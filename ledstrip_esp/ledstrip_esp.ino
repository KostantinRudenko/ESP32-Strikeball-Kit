#pragma region ______________________________ Includes

#include "FastLED.h"
#include <WiFi.h>
#include <esp_now.h>
#include "macs.h"

#pragma endregion Includes

# pragma region ______________________________ Constants

const uint8_t LEDSTRIP_PIN     = 5;

const uint16_t LEDS_AMOUNT     = 10;
const uint8_t BRIGHTNESS       = 200;
const uint8_t MAX_LED_CONTRAST = 255;

const uint8_t MAX_PROGRESS = 100;

#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

#define LED_OFF      CRGB(0,0,0)

#define RED_RGB      CRGB(255,0,0)
#define GREEN_RGB    CRGB(0,255,0)
#define BLUE_RGB     CRGB(0,0,255)
#define WHITE_RGB    CRGB(255,255,255)

#define RED_H    0
#define GREEN_H  85
#define BLUE_H   170

/* CRGB (красный, зеленый, синий)
 * CHSV (цвет, насыщенность, яркость)
 * Красный - 255
 * Синий - 170
 * Зеленый - 85
*/

const String Gc_sGamepadMAC = "B0:A7:32:2A:BA:10";

const uint8_t RED_TEAM_NUM = 0;
const uint8_t BLUE_TEAM_NUM = 1;

const uint16_t ONE_SECOND_DELAY = 1000;

#pragma endregion Constants

#pragma region ______________________________ Variables

enum TaskMainStates {
    ST_INIT = 0,
    ST_WAIT_CMD,
    ST_SEND_ACK
};

enum TaskWifiStates {
    ST_WIFI_INIT = 0,
    ST_WIFI_RUN,
    ST_WIFI_ERROR
};

enum ModuleStates {
  NOT_CONNECTED = 0,
  STRIP_BUSY,
  STRIP_READY,
  UNKNOWN_CMD
};

enum Commands {
    Ping = 1,
    FillStrip,
    BlinkStrip
};

enum Teams {
    RedTeam = 0,
    BlueTeam
};

String G_sThisDeviceMAC;
uint8_t G_aru8ThisDeviceMAC[6];
uint8_t G_aruGamepadMAC[6];

CRGB leds[LEDS_AMOUNT];

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

typedef struct {
    uint8_t cmd;
    uint8_t data[4];
} espnow_msg_t;

#pragma endregion Variables

uint16_t ConvertProgressToLedsAmount(uint8_t& progress) {
    return map(progress, 0, MAX_PROGRESS, 0, LEDS_AMOUNT);
}

void FillAllLedsWithColor(const CRGB& color) {
  fill_solid(leds, LEDS_AMOUNT, color);
}

void BlinkAllLedsWithColorAndDelay(const CRGB& color, const uint16_t& delayTime) {
    FillAllLedsWithColor(WHITE_RGB);
    delay(delayTime);
    FillAllLedsWithColor(LED_OFF);
}

bool FillStripWithTeamColorByProgressCustomDelay(uint8_t team, uint8_t progress, const uint16_t& delayTime) {
    if (team > 1 || 0 > progress > MAX_PROGRESS) {
        return false;
    }

    uint8_t teamColor;
    if (team == 0) { teamColor = RED_H; } else { teamColor = BLUE_H; }
    uint16_t progressLeds = ConvertProgressToLedsAmount(progress);

    for (uint16_t ledNum=0; ledNum<progressLeds; ledNum++) {
        leds[ledNum] = CHSV(0, 200, 200);
        FastLED.show();
        delay(delayTime);
    }
    return true;
}

void setup() {
    pinMode(LEDSTRIP_PIN, OUTPUT);
    FastLED.addLeds<LED_TYPE, LEDSTRIP_PIN, COLOR_ORDER>(leds, LEDS_AMOUNT);

    BlinkAllLedsWithColorAndDelay(WHITE_RGB, ONE_SECOND_DELAY);
}

void loop() {
  FillStripWithTeamColorByProgressCustomDelay(RedTeam, 50, ONE_SECOND_DELAY);
  FillAllLedsWithColor(LED_OFF);
  delay(1000);
}
