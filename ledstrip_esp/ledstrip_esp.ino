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
 * Красный - 0
 * Синий - 170
 * Зеленый - 85
*/

const String Gc_sGamepadMAC = "B0:A7:32:2A:BA:10";

const uint8_t RED_TEAM_NUM = 0;
const uint8_t BLUE_TEAM_NUM = 1;

const uint16_t ONE_SECOND_DELAY = 1000;


const uint8_t NTF_SEND_WIFI       = 0b00000001;
const uint8_t NTF_RECV_WIFI       = 0b00000010;
const uint8_t NTF_SEND_OK_WIFI    = 0b00000100;
const uint8_t NTF_SEND_FAIL_WIFI  = 0b00001000;

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
    FillStrip = 3,
    FillStripiByProgress,
    ClearStrip
};

enum Teams {
    RedTeam = 0,
    BlueTeam
};

bool Gx_arLedsStates[LEDS_AMOUNT];

String G_sThisDeviceMAC;
uint8_t G_aru8ThisDeviceMAC[6];
uint8_t G_aru8GamepadMAC[6];

uint8_t G_u8MainState = ST_INIT;
uint8_t G_u8WiFiState = ST_WIFI_INIT;
bool G_xDeviceConnected = false;

CRGB leds[LEDS_AMOUNT];

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

typedef struct {
    uint8_t cmd;
    uint8_t data[2];
} msg_esp_now_t;

esp_now_peer_info_t peerInfo;

QueueHandle_t queue_in;
msg_esp_now_t outMsg;

#pragma endregion Variables

#pragma region ______________________________ Functions

void Clear();

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

void FillStripByProgress(const uint8_t& teamColor, const uint8_t& progress) {
    uint16_t ledsAmount = ConvertProgressToLedsAmount(progress);
    CRGB color = RED_TEAM_NUM ? RED_RGB : BLUE_RGB;
    LightLedsFromStart(ledsAmount, color);
}

void StartStripAnimation() {
    LightLedsFromStart(LEDS_AMOUNT, WHITE_RGB);
    delay(ONE_SECOND_DELAY);
    Clear();
}

void Clear() {
    LightLedsFromStart(LEDS_AMOUNT, LED_OFF);
}

#pragma endregion Functions

#pragma region ______________________________ Main

bool sendESP_NOW(bool* res, msg_esp_now_t* msg) {
    //----------------------------------------------------------------------------+
    //                  Передать сообщение через ESP_NOW                          |
    //  [in] msg - указатель на сообщение                                         |
    //  [in/out] - возвращает true, если передано успешно, иначе false            |
    //  return   - true, если передача заверщена, иначе false                     |
    //----------------------------------------------------------------------------+
    static uint8_t st = 0;
    BaseType_t rc;
    uint32_t rv;

    if (st == 0) {
        rc = xTaskNotify(hTaskWiFi, NTF_SEND_WIFI, eSetBits);
        st = 1;
    }
  else {
        rc = xTaskNotifyWait(0, NTF_SEND_OK_WIFI | NTF_SEND_FAIL_WIFI, &rv, 0);
        if (rc == pdTRUE)
        {
            if (rv & NTF_SEND_OK_WIFI)
                *res = true;
            else if (rv & NTF_SEND_FAIL_WIFI) {
                log_e("Error sending the data");
                *res = false;
            }
            st = 0;
            return true;
        }   // pdFALSE - если тайм-аут
    }
    return false;
}

void useLed(uint8_t command, const msg_esp_now_t& params) {
    switch (command) {
        case FillStripiByProgress:
            FillStripByProgress(params.data[0], params.data[1]);
            break;

        case ClearStrip:
            Clear();
            break;
    }
}

bool parseMessage(msg_esp_now_t* rec_msg, msg_esp_now_t* ack_msg) {
    /*
    Разбор принятого сообщения
    Формирование ответного сообщения
    rec_msg - указатель на принятое сообщение
    ack_msg - указатель на ответное сообщение
    Возвращает true, если необходимо передать ответ
    */
    if (rec_msg->cmd == Ping) {
        ack_msg->cmd = rec_msg->cmd;
        ack_msg->data[0] = rec_msg->data[0];
        return true;
    }

    useLed(rec_msg->cmd, *rec_msg);
    return false;
}

void TaskMain(void *pvParameters) {
    uint32_t rv;
    BaseType_t s;
    msg_esp_now_t qitem;
    bool result;

    for (;;) {
        switch (G_u8MainState) {
            case ST_INIT:           // ждем уведомления о подключении WiFi и готовности ESP-NOW
                rv = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (rv)
                    G_u8MainState++;
                break;

            case ST_WAIT_CMD:       // ожидание сообщения, парсинг
                // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
                // s = xQueueReceive(queue_in, &qitem, 0);
                s = xQueueReceive(queue_in, &qitem, portMAX_DELAY);
                // если данные из очереди получены
                if (s == pdPASS) {
                    // выполнение команды, подготовка ответного сообщения
                    if (parseMessage(&qitem, &outMsg))
                        G_u8MainState++;
                }
                break;

            case ST_SEND_ACK:          //  передача оветного сообщения
                if (sendESP_NOW(&result, &outMsg))
                    G_u8MainState = ST_WAIT_CMD;
                break;
        }
    }
}

#pragma endregion Main

#pragma region ______________________________ WiFi

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS)
        xTaskNotify(hTaskMain, NTF_SEND_OK_WIFI, eSetBits);
    else
        xTaskNotify(hTaskMain, NTF_SEND_FAIL_WIFI, eSetBits);
}

void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
    msg_esp_now_t qitem;

    memcpy(&qitem, incomingData, sizeof(msg_esp_now_t));
    if (xQueueSendToBack(queue_in, &qitem, 0) != pdPASS)
        log_e("Input queue overflow");
}

bool connectToWiFi() {
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        log_i("Error in esp_now_init");
        return false;
    }

    if (esp_now_register_send_cb(OnDataSent) != ESP_OK) {
        log_e("Error in esp_now_register_send_cb");
        return false;
    }

    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, G_aru8GamepadMAC, 6);

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        log_e("Error in esp_now_add_peer");
        return false;
    }

    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        log_i("Error in esp_now_register_recv_cb");
        return false;
    }
    return true;
}

void TaskWiFi(void* pvParameters) {
    uint32_t rv;
    BaseType_t rc;

    disableCore0WDT();

    for (;;) {
        switch (G_u8WiFiState) {
            case ST_WIFI_INIT:
                if (connectToWiFi()) {
                    xTaskNotifyGive(hTaskMain);
                    G_u8WiFiState = ST_WIFI_RUN;
                }
                else
                    G_u8WiFiState = ST_WIFI_ERROR;
                break;

            case ST_WIFI_RUN:
                if (xTaskNotifyWait(0, NTF_SEND_WIFI, &rv, 0) == pdTRUE) {
                    esp_now_send(G_aru8GamepadMAC, (uint8_t *) &outMsg, sizeof(msg_esp_now_t));
                    break;
                }

            case ST_WIFI_ERROR:
                break;
        }
    }
}

#pragma endregion WiFi

void setup() {
    pinMode(LEDSTRIP_PIN, OUTPUT);
    FastLED.addLeds<LED_TYPE, LEDSTRIP_PIN, COLOR_ORDER>(leds, LEDS_AMOUNT);

    G_sThisDeviceMAC = WiFi.macAddress();

    MacStringToByteArray(G_sThisDeviceMAC, G_aru8ThisDeviceMAC);
    MacStringToByteArray(Gc_sGamepadMAC, G_aru8GamepadMAC);

    queue_in = xQueueCreate(10, sizeof(msg_esp_now_t));
    assert(queue_in);
    if (queue_in != NULL) {
        delay(ONE_SECOND_DELAY);
        StartStripAnimation();
        xTaskCreatePinnedToCore(TaskMain, "TaskMain", 10000, NULL, 1, &hTaskMain, 1);
        xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 20000, NULL, 2, &hTaskWiFi, 0);
        //xTaskCreatePinnedToCore(, "TaskMain", 1000, NULL, 1, &, 1);
        //xTaskCreatePinnedToCore();
    }
}

void loop() {}
