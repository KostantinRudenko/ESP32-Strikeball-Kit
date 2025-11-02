#pragma region ______________________________ Constants

#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

#define LEDS_OFF      CRGB(0,0,0)

#define RED_RGB      CRGB(255,0,0)
#define GREEN_RGB    CRGB(0,255,0)
#define BLUE_RGB     CRGB(0,0,255)
#define WHITE_RGB    CRGB(255,255,255)

#define RED_H    0
#define GREEN_H  85
#define BLUE_H   170

const char* Gc_sGamepadMAC = "80:F3:DA:61:AC:70";

/* CRGB (красный, зеленый, синий)
 * CHSV (цвет, насыщенность, яркость)
 * Красный - 0
 * Синий - 170
 * Зеленый - 85
*/

const uint8_t LEDSTRIP_PIN = 5;


const uint16_t LEDS_AMOUNT                = 10;
const uint8_t START_ANIMATION_LEDS_AMOUNT = 5;

const uint8_t BRIGHTNESS       = 200;
const uint8_t MAX_LED_CONTRAST = 255;

const uint8_t MAX_PROGRESS = 100;

const uint16_t ONE_SECOND_DELAY = 1000;

const uint8_t NTF_SEND_WIFI       = 0b00000001;
const uint8_t NTF_RECV_WIFI       = 0b00000010;
const uint8_t NTF_SEND_OK_WIFI    = 0b00000100;
const uint8_t NTF_SEND_FAIL_WIFI  = 0b00001000;

#pragma endregion Constants

#pragma region ______________________________ StructuresAndEnums

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
    CMD_FillStrip = 3,
    CMD_FillStripByProgress,
    CMD_ClearStrip
};

enum Teams {
    RedTeam = 0,
    BlueTeam
};

typedef struct {
    uint8_t cmd;     // каманда
    uint8_t data[3]; // data[0] - номер peer: (0 - red; 1 - blue; 2 - ledStrip; 255 - broadcast;)
                     // data[1] - цвет команды
                     // data[2] - прогресс заполнения ленты(опционально, только для команды CMD_FillStripByProgress)
} msg_esp_now_t;

typedef struct {
    uint8_t id_cb;          // callback-функция, поместившая сообщение в очередь
    uint8_t status;         // состояние
    espnow_msg_t msg;       // сообщение
} espnow_event_t;

#pragma endregion Structures
