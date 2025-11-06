
#pragma region ________________________________ Includes

#include <WiFi.h>
#include <esp_now.h>
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "FastLED.h"
#include "macs.h"

#pragma endregion Includes

#pragma region ________________________________ Constants

const String Gc_sGamepadMAC = "80:F3:DA:61:AC:70";                // MAC адрес игрового пульта в формате строки "12:34:56:78:9A:BC"

const uint8_t LED_STRIP_PIN       = 15;                           // управляющий пин светодиодной ленты
const uint8_t SIREN_PIN           = 17;                           // управляющий пин сирены(идет на реле)

const uint8_t NTF_SEND_WIFI       = 0b00000001;
const uint8_t NTF_RECV_WIFI       = 0b00000010;
const uint8_t NTF_SEND_OK_WIFI    = 0b00000100;
const uint8_t NTF_SEND_FAIL_WIFI  = 0b00001000;

#define LED_PIN      15 
#define LEDS_NUM     10
#define BRIGHTNESS   50
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

#define MAX_PROGRESS 100

#define LED_OFF      CRGB(0,0,0)
#define WHITE_RGB    CRGB(255,255,255)
#define RED_RGB      CRGB(255,0,0)
#define BLUE_RGB     CRGB(0,0,255)

#define NOT_CLEAR    false

#pragma endregion Constants

#pragma region ________________________________ Variables

enum task_main_states_t {
  ST_INIT = 0,
  ST_WAIT_CMD,
  ST_SEND_ACK
};

enum task_wifi_states_t {
  ST_WIFI_INIT=0,
  ST_WIFI_RUN,
  ST_WIFI_ERROR
};

enum Commands {
  Ping = 1,                 // сканирование
  LightLeds = 3,
  LightLedsByProgress
};

enum base_states_t {
  PEER_NO_CONNECT = 0,      // точка не подключена
  DEVICE_NO_INIT,           // не инициализирован
  DEVICE_BUSY,              // занят другой командой
  DEVICE_READY,             // готов
  UNKNOUWN_CMD              // неизвестная команда
};

typedef struct msg_esp_now_t {
  uint8_t cmd;
  uint8_t data[3]; 
} msg_esp_now_t;

String G_sThisDeviceMAC;                                          // MAC адрес этого устройства в формате строки "12:34:56:78:9A:BC"
uint8_t G_aru8ThisDeviceMAC[6];                                   // MAC адрес этого устройства в формате массива байт
uint8_t G_aru8GamepadMAC[6];                                      // MAC адрес игрового пульта в формате массива байт

esp_now_peer_info_t peerInfo;
QueueHandle_t queue_in;                                           // очередь входящих сообщений

uint8_t G_u8MainState = ST_INIT;                                  // Состояние главной задачи
uint8_t G_u8WiFiState = 0;                                        // Состояние задачи WiFi

CRGB leds[LEDS_NUM];
bool isLedInitialized = false;

msg_esp_now_t outMsg;

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

#pragma endregion Variables

#pragma region ________________________________ Functions

void initLedStrip() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, LEDS_NUM);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, LEDS_NUM, LED_OFF);
  FastLED.show();
}

void lightLeds(uint16_t amount, CRGB color, bool isClear=true) {
  if (isClear) {
    fill_solid(leds, LEDS_NUM, LED_OFF);
    }
  fill_solid(leds, amount, color);
  FastLED.show();
}

void lightLedsByProgress(uint8_t progress, CRGB color, bool isClear=true) {
  if (isClear) {
    fill_solid(leds, LEDS_NUM, LED_OFF);
    }
  fill_solid(leds,
             map(progress, 0, MAX_PROGRESS, 0, LEDS_NUM),
             color);
  FastLED.show();
}

void useLedStrip(msg_esp_now_t* msg) {
  switch (msg->cmd) {
    case LightLeds:
      log_i("Light leds command");
      lightLeds(LEDS_NUM, msg->data[1]);
    case LightLedsByProgress:
      log_i("Light leds by progress command: progress - %d, team - %d", msg->data[2], msg->data[1] == 0 ? "NOONE" : (msg->data[1] == 1 ? "RED" : "BLUE");
      lightLedsByProgress(msg->data[2], msg->data[1] == 0 ? RED_RGB : BLUE_RGB);
  }
}
#pragma endregion Functions


#pragma region ________________________________ Main_task


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
        ack_msg->data[1] = isLedInitialized ? DEVICE_READY : DEVICE_NO_INIT;

        log_i("Состояние = %d", ack_msg->data[1]);
        return true;    
    }

    useLedStrip(rec_msg);
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


void loop() {}

#pragma endregion Main_task


#pragma region ________________________________ WiFi_task

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS)
        xTaskNotify(hTaskMain, NTF_SEND_OK_WIFI, eSetBits);
    else
        xTaskNotify(hTaskMain, NTF_SEND_FAIL_WIFI, eSetBits);
}


void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    msg_esp_now_t qitem;

    memcpy(&qitem, incomingData, sizeof(msg_esp_now_t));
    // помещаем элемент в конец очереди и не ждем, если очередь переполнена
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


void TaskWiFi(void *pvParameters) {
    uint32_t rv;
    BaseType_t rc;
  
    disableCore0WDT();

    for (;;) {
        switch (G_u8WiFiState) { 
            case ST_WIFI_INIT:                      // Подключение...
                if (connectToWiFi()) {
                    xTaskNotifyGive(hTaskMain);
                    G_u8WiFiState = ST_WIFI_RUN;
                }
                else
                    G_u8WiFiState = ST_WIFI_ERROR;
                break;

            case ST_WIFI_RUN:                          // Работа
                // ждем событие передать сообщение или отключить WIFI
                // Значение слова события возвращается в rv, которое затем проверяется и в зависимости от
                // установленного бита(ов), мы выполняем команду.
                // может установить задержку в portMAX_DELAY ?????????????????????????????????????????????????????
                
                // rc = xTaskNotifyWait(0, NTF_SEND_WIFI, &rv, portMAX_DELAY);
                // rc = xTaskNotifyWait(0, NTF_SEND_WIFI, &rv, 0);
                if (xTaskNotifyWait(0, NTF_SEND_WIFI, &rv, 0) == pdTRUE)
                    esp_now_send(G_aru8GamepadMAC, (uint8_t *) &outMsg, sizeof(msg_esp_now_t));
                break;

            case ST_WIFI_ERROR:
                break;
        }
    }
}

#pragma endregion WiFi_task


#pragma region ________________________________ Setup_function

void setup() { 
    // Serial.begin(115200);
    log_i("Base start.");
    
    esp_task_wdt_deinit();

    initLedStrip();
    isLedInitialized = true;

    log_i("Inited led Strip");

    G_sThisDeviceMAC = WiFi.macAddress();
    MacStringToByteArray(G_sThisDeviceMAC, G_aru8ThisDeviceMAC);
    MacStringToByteArray(Gc_sGamepadMAC, G_aru8GamepadMAC);

    // Length (with one extra character for the null terminator)
    int8_t str_len = G_sThisDeviceMAC.length() + 1; 
    // Prepare the character array (the buffer) 
    char char_array[str_len];
    // Copy it over 
    G_sThisDeviceMAC.toCharArray(char_array, str_len);
    log_i("G_sThisDeviceMAC = %s", char_array);


    queue_in = xQueueCreate(10, sizeof(msg_esp_now_t));
    assert(queue_in);
    if (queue_in != NULL) {
        xTaskCreatePinnedToCore(TaskMain, "TaskMain", 10000, NULL, 1, &hTaskMain, 1);
        xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 20000, NULL, 2, &hTaskWiFi, 0);
    }  
}

#pragma endregion Setup_function

