#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#pragma once

#pragma region ________________________________ Includes

#include <WiFi.h>
#include <esp_now.h>
//#include <esp_wifi.h>
//#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Preferences.h>
#include <ESP32Servo.h>           // для работы beeper
#include <TFT_eSPI.h>
#include "TimerExt.h"
#include "message_esp_now.h"

#include "fonts/teutonnormal20pt7b.h"
#include "fonts/teutonnormal16pt7b.h"

#pragma endregion Includes


#pragma region ________________________________ Constants

// логические уровни сигналов управления светодиодами
#define LED_ON  HIGH      // LOW
#define LED_OFF LOW       // HIGH

/* НАСТРОЙКА TFT SPI
 * Для настройки нужно найти место где находиться библиотека и редактировать User_Setup.h.
 * Нужно настроить подксветку, инверсию, драйвер(!), пины(!), частоту(!)
 * ! - обязательно
 *
 * (это поверхностно, для деталей надо смотреть сам файл, там есть инструкции)
 *
 * При странно поведении дисплэя, проверить напряжение: должно быть 3.3В
 */

//Pins
//const uint8_t LINK_LED_PIN    = 4;                                // светодиод LINK
const uint8_t BUZZER_PIN      = 13;                               // пищалка
const uint8_t BUTTON_RED_PIN  = 22;                               // кнопка красной команды
const uint8_t BUTTON_BLUE_PIN = 21;                               // кнопка синей команды
//const uint8_t RED_LED_PIN     = 16;                               // подсветка кнопки красной команды
//const uint8_t BLUE_LED_PIN    = 17;                               // подсветка синей красной команды

//const int8_t LCD_ROWS = 4;                                        // число строк дисплея

// const uint8_t MAX_POINTS = 2;                                     // Мах количество точек

#define I2C_Addr_LCD 0x27

const uint8_t BUZZER_PWM_CHANNEL = 0;                             // ШИМ канал для буззера (всего доступно 16 (0-15) каналов)
const uint16_t BUZZER_FREQUENCY = 200;
const uint16_t BUZZER_BUTTON = 1500;                              // частота при нажатии кнопки
const uint16_t BUZZER_DURATION = 30;                              // длительность пика при нажатии кнопки

const uint32_t DELAY_START = 60000;                                 // задержка автозапуска игры во всех режимах (1 минута)

const uint8_t DLG_NONE = 0;
const uint8_t DLG_NO = 1;
const uint8_t DLG_YES = 2;

#define ROW_NUM     4
#define COLUMN_NUM  4

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

//const String TEXT_FONT_FILE = "teutonnormal36";
//const String HEADER_FONT_FILE = "Teutonnormal68"; // тут шрифт с большой буквы

const uint8_t SPACE = 12; // пиксели между строк

#define HEADER_FONT &teutonnormal20pt7b
#define STRING_FONT &teutonnormal16pt7b

uint8_t TEXT_HEIGHT = *STRING_FONT.yAdvance;
uint8_t HEADER_HEIGHT = *HEADER_FONT.yAdvance;

const uint8_t HEADER_SPACE_H = HEADER_HEIGHT + SPACE;
const uint8_t STRING_SPACE_H = TEXT_HEIGHT + SPACE;

#define CENTER_BY_X true
#define CENTER_BY_Y true

#define NOT_CENTER_BY_X false
#define NOT_CENTER_BY_Y false

#define NO_X 0
#define NO_Y 0

const uint16_t DISPLAY_WIDTH = 480;
const uint16_t DISPLAY_HEIGHT = 320;

const uint16_t PROGRESS_BAR_WIDTH = 420;
const uint16_t PROGRESS_BAR_HEIGHT = 50;

const uint16_t PROGRESS_BAR_X_POSITION = (DISPLAY_WIDTH-PROGRESS_BAR_WIDTH)/2;
const uint16_t PROGRESS_BAR_Y_POSITION = HEADER_HEIGHT*2-PROGRESS_BAR_HEIGHT-SPACE/2;

#define DEFAULT_TEXT_COLOR TFT_WHITE
#define CHOOSEN_TEXT_COLOR TFT_SILVER

#pragma endregion Constants


#pragma region ________________________________ Variables

enum team_t {NOONE=0, RED, BLUE};

enum modes {DOMIN, DOMIN_PRO, BOMB, CTRL_POINT, EDIT_PARAMS};           // режимы
const int8_t NUM_MODES = EDIT_PARAMS - DOMIN + 1;                  // число режимов

enum gstates_t {
    ST_GREET = 0,
    ST_CHECKPARS,
    ST_GAMEMODE,
    ST_OLDPARS,
    ST_EDIT_PARS,
    ST_SAVEPARS,
    ST_PARS2PLAY,
    ST_PRESSANYKEY,
    ST_DELAY_START,
    ST_DOMIN,
    ST_DOMIN_PRO,
    ST_BOMB,
    ST_CTRL_POINT,
    ST_RESULT_SCREEN,             // Вывод результата игры
    ST_WAIT_RESET
};


enum point_states_t {
    PEER_NO_CONNECT = 0,      // точка не подключена
    PLAYER_NO_INIT,           // плейер не инициализирован
    PLAYER_BUSY,              // играет трек
    PLAYER_READY,             // готов
    UNKNOUWN_CMD              // неизвестная команда
};

const char *sPointNameStates[5]  = {
  //01234567890123456789
  //BLUE:
         "Disconnect    ",
         "Player bad    ",
         "Player busy   ",
         "Player ready  ",
         "Bad command   "
};



TFT_eSPI tft = TFT_eSPI();
//LiquidCrystal_I2C lcd(I2C_Addr_LCD, 20, LCD_ROWS);

uint8_t pin_rows[ROW_NUM] = {14, 27, 26, 25};
uint8_t pin_column[COLUMN_NUM] = {33, 32, 16, 15};

Keypad kpd = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

char key;                                 // Буфер клавиатуры

uint8_t G_aru8MACs[MAX_POINTS+1][ESP_NOW_ETH_ALEN] = {
    {0,0,0,0,0,0},
    {0,0,0,0,0,0},
	{0,0,0,0,0,0},
	{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
};          // МАС адреса точек

static QueueHandle_t queue;

Button blueButton(BUTTON_BLUE_PIN, 50, true, true);
Button redButton(BUTTON_RED_PIN, 50, true, true);

uint8_t G_u8DeviceState = 0;                    // Состояние устройства
modes G_u8GameMode = DOMIN;                     // Режим (0..3)
// TimerExt game_timer(false);                   // Обратный таймер времени игры
uint32_t G_u32GameTimeMS;                       // Время игры
uint32_t G_u32ActivationTimeMS;                 // Время активации игрока - мин время в раб.зоне
uint32_t G_u32BombTimeMS;                       // Время бомбы
uint32_t G_u32RepeatTimeMS;                     // Время блокировки повторного нажатия кнпки в режиме "CONTROL POINT"
String G_sPassword = "00000000";                // Пароль
uint8_t G_arPeerStatus[MAX_POINTS];             // Состояния точек (true - точка найдена, иначе false)


bool xLink;

#pragma endregion Variables

#endif
