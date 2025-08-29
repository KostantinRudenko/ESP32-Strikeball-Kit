#include "FastLED.h"
//#include "global.h"

#pragma region ______________________________ Constants


#pragma endregion Constants

#pragma region ______________________________ Variables

CRGB leds[LEDS_AMOUNT];

#pragma endregion Variables

#pragma region ______________________________ Functions

void ClearStrip();

uint16_t ConvertProgressToLedsAmount(const uint8_t& progress) {
    return map(progress, 0, MAX_PROGRESS, 0, LEDS_AMOUNT);
}

void LightLedsFromStart(const uint16_t& amount, const CHSV& color) {
    fill_solid(leds, amount, color);
    FastLED.show();
}

void LightLedsFromStart(const uint16_t& amount, const CRGB& color) {
    fill_solid(leds, amount, color);
    FastLED.show();
}

void FillStripByProgress(const uint8_t& teamNumber, const uint8_t& progress) {
    uint16_t ledsAmount = ConvertProgressToLedsAmount(progress);
    CRGB color = (teamNumber == 0) ? RED_RGB : BLUE_RGB; // 0 - red team; 1 - blue team
    LightLedsFromStart(ledsAmount, color);
}

void StartStripAnimation() {
    LightLedsFromStart(START_ANIMATION_LEDS_AMOUNT, WHITE_RGB);
    vTaskDelay(ONE_SECOND_DELAY);
    ClearStrip();
}

void ClearStrip() {
    LightLedsFromStart(LEDS_AMOUNT, LED_OFF);
}

void InitLedStrip() {
    FastLED.addLeds<LED_TYPE, LEDSTRIP_PIN, COLOR_ORDER>(leds, LEDS_AMOUNT);
}

#pragma endregion Functions
