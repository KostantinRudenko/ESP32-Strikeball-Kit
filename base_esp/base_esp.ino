const String Gc_sGamepadMAC = "B0:A7:32:2A:BA:10";                // MAC адрес игрового пульта в формате строки "12:34:56:78:9A:BC"
/*================================================================
                   Модуль базы
Принимает по WiFi номер трека от игрового модуля и проигрывает его
на плейере. Это устройство является ведомым.
Отвечает только на адресное сообщение Ping.
------------------------------------------------------------------
направление передачи:   gamepad => base
cmd = Ping
data[0] = номер базы 0-RED; 1-BLUE
data[1] = 

Ответ    gamepad <= base
cmd = Ping
data[0] = номер базы 0-RED; 1-BLUE
data[1] = состояние плейера 1-не инициализирован; 2-играет трек; 3-готов; 4-неизвестная команда
------------------------------------------------------------------
направление передачи:   gamepad => base
cmd = PlayTrack (проиграть трек)
data[0] = номер базы
data[1] = номер трека
------------------------------------------------------------------
 V01 07-apr-2024
================================================================*/

#pragma region ________________________________ Constants

const uint8_t AUDIO_VOLUME = 30;                                  // Громкость плейера 30 (максимум)
const uint16_t Gc_u16BusyCheckDelayMS = 400;                      // Задержка опроса состояния пина Busy плейера

// логические уровни сигналов управления светодиодами
#define LED_ON  HIGH      // LOW
#define LED_OFF LOW       // HIGH

const uint8_t LINK_LED_PIN    = 4;                                // светодиод LINK
const uint8_t PLAYER_BUSY_PIN = 19;                               // сюда подключен вывод BUSY DFPlayerMini
const uint8_t PLAYER_TX_PIN   = 16;                               // сюда подключен вывод TX DFPlayerMini 
const uint8_t PLAYER_RX_PIN   = 17;                               // сюда подключен вывод RX DFPlayerMini 

const uint8_t NTF_SEND_WIFI       = 0b00000001;
const uint8_t NTF_RECV_WIFI       = 0b00000010;
const uint8_t NTF_SEND_OK_WIFI    = 0b00000100;
const uint8_t NTF_SEND_FAIL_WIFI  = 0b00001000;

#pragma endregion Constants


#pragma region ________________________________ Includes

#include <WiFi.h>
#include <esp_now.h>
#include <DFRobotDFPlayerMini.h>
#include "macs.h"

#pragma endregion Includes


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
  PlayTrack                 // проиграть трек
};

enum base_states_t {
  PEER_NO_CONNECT = 0,      // точка не подключена
  PLAYER_NO_INIT,           // плейер не инициализирован
  PLAYER_BUSY,              // играет трек
  PLAYER_READY,             // готов
  UNKNOUWN_CMD              // неизвестная команда
};

typedef struct msg_esp_now_t {
  uint8_t cmd;
  uint8_t data[2]; 
} msg_esp_now_t;

String G_sThisDeviceMAC;                                          // MAC адрес этого устройства в формате строки "12:34:56:78:9A:BC"
uint8_t G_aru8ThisDeviceMAC[6];                                   // MAC адрес этого устройства в формате массива байт
uint8_t G_aru8GamepadMAC[6];                                      // MAC адрес игрового пульта в формате массива байт

DFRobotDFPlayerMini audio;
bool G_xAudioConnected;                                           // true=DFPlayerMini connected

bool G_xLink = LED_OFF;                                           // состояние светодиода LINK
uint32_t G_u32LinkLEDTime = 0;                                    // таймер подсвечивания светодиода LINK
esp_now_peer_info_t peerInfo;
QueueHandle_t queue_in;                                           // очередь входящих сообщений

uint8_t G_u8MainState = ST_INIT;                                  // Состояние главной задачи
uint8_t G_u8WiFiState = 0;                                        // Состояние задачи WiFi

msg_esp_now_t outMsg;

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

#pragma endregion Variables


#pragma region ________________________________ Main_task

bool playTrack(uint8_t track, bool force = false) {
    static uint32_t tm = 0;  
    if (G_xAudioConnected) {
        if (!force && (xTaskGetTickCount() < tm || !digitalRead(PLAYER_BUSY_PIN)))
            return false;
        audio.play(track);
        tm = xTaskGetTickCount() + Gc_u16BusyCheckDelayMS;
    }
    return true;
}


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
        ack_msg->data[1] = G_xAudioConnected ? digitalRead(PLAYER_BUSY_PIN) + PLAYER_BUSY : PLAYER_NO_INIT;
        log_i("Состояние = %d", ack_msg->data[1]);
        return true;    
    }

    playTrack(rec_msg->data[1], true);
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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS)
        xTaskNotify(hTaskMain, NTF_SEND_OK_WIFI, eSetBits);
    else
        xTaskNotify(hTaskMain, NTF_SEND_FAIL_WIFI, eSetBits);
}


void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    msg_esp_now_t qitem;

    memcpy(&qitem, incomingData, sizeof(msg_esp_now_t));
    // помещаем элемент в конец очереди и не ждем, если очередь переполнена
    if (xQueueSendToBack(queue_in, &qitem, 0) != pdPASS) 
        log_e("Input queue overflow");  
    
    G_xLink = LED_ON;
    digitalWrite(LINK_LED_PIN, G_xLink);
    G_u32LinkLEDTime = xTaskGetTickCount();
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

        // Светодиод LINK_LED держим включенным не более 50 мс
        if (G_xLink && G_u8WiFiState) {
            if (xTaskGetTickCount() - G_u32LinkLEDTime > 50) {
                G_xLink = LED_OFF;
                digitalWrite(LINK_LED_PIN, G_xLink);
            }
        }
    }
}

#pragma endregion WiFi_task


#pragma region ________________________________ Setup_function

void setup() { 
    // Serial.begin(115200);
    log_i("Base start.");

    pinMode(LINK_LED_PIN, OUTPUT);
    digitalWrite(LINK_LED_PIN, LED_OFF);

    pinMode(PLAYER_BUSY_PIN, INPUT);

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

    delay(1500);  // добавлено, иначе не запускается плейер

    Serial2.begin(9600, SERIAL_8N1, /*rx =*/PLAYER_TX_PIN, /*tx =*/PLAYER_RX_PIN);

    delay(1500);

    G_xAudioConnected = audio.begin(Serial2, true, true);

    if (G_xAudioConnected) {
        audio.volume(AUDIO_VOLUME);
        // Короткий ~ 4 с трек. Проигрываетя при включении. Показывает, что плейер запущен успешно.
        audio.play(1);
        log_i("DFPlayer start OK");
    }
    else
        log_e("Error DFPlayer init.");  
    
    delay(5000);

    queue_in = xQueueCreate(10, sizeof(msg_esp_now_t));
    assert(queue_in);
    if (queue_in != NULL) {
        xTaskCreatePinnedToCore(TaskMain, "TaskMain", 10000, NULL, 1, &hTaskMain, 1);
        xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 20000, NULL, 2, &hTaskWiFi, 0);
    }  
}

#pragma endregion Setup_function
