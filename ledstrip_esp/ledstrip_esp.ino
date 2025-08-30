#pragma region ______________________________ Includes

#include <WiFi.h>
#include <esp_now.h>
#include "global.h"
#include "macs.h"
#include "functions.h"

#pragma endregion Includes
#pragma region ______________________________ Variables

String G_sThisDeviceMAC;
uint8_t G_aru8ThisDeviceMAC[6];
uint8_t G_aru8GamepadMAC[6];

uint8_t G_u8MainState = ST_INIT;
uint8_t G_u8WiFiState = ST_WIFI_INIT;
// bool G_xDeviceConnected = false;

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

esp_now_peer_info_t peerInfo;

QueueHandle_t queue_in;
msg_esp_now_t outMsg;

#pragma endregion Variables

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

void useLed(uint8_t command, msg_esp_now_t* params) {
    switch (command) {
        case CMD_FillStripByProgress:
            FillStripByProgress(params->data[0], params->data[1]);
            break;

        case CMD_ClearStrip:
            ClearStrip();
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

    useLed(rec_msg->cmd, rec_msg);
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
                if (rv) {
                    G_u8MainState++;
                    Serial.println("main initialized");
                }
                break;

            case ST_WAIT_CMD:       // ожидание сообщения, парсинг
                // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
                // s = xQueueReceive(queue_in, &qitem, 0);
                s = xQueueReceive(queue_in, &qitem, portMAX_DELAY);
                // если данные из очереди получены
                if (s == pdPASS) {
                    // выполнение команды, подготовка ответного сообщения
                    if (parseMessage(&qitem, &outMsg))
                        //G_u8MainState++;
                        G_u8MainState = ST_WAIT_CMD;
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

                    Serial.println("wifi initialized");

                    //LightLedsFromStart(1, GREEN_RGB);
                    //vTaskDelay(ONE_SECOND_DELAY);
                    //ClearStrip();
                }
                else {

                    G_u8WiFiState = ST_WIFI_ERROR;
                  }
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
    Serial.begin(9600);
    Serial.println("\nserial initialized");

    pinMode(LEDSTRIP_PIN, OUTPUT);

    G_sThisDeviceMAC = WiFi.macAddress();

    InitLedStrip();
    FastLED.addLeds<LED_TYPE, LEDSTRIP_PIN, COLOR_ORDER>(leds, LEDS_AMOUNT);
    Serial.println("v1");

    Serial.println("fastled initialized");

    MacStringToByteArray(G_sThisDeviceMAC, G_aru8ThisDeviceMAC);
    MacStringToByteArray(Gc_sGamepadMAC, G_aru8GamepadMAC);

    queue_in = xQueueCreate(10, sizeof(msg_esp_now_t));
    assert(queue_in);
    if (queue_in != NULL) {

        StartStripAnimation();
        ClearStrip();

        xTaskCreatePinnedToCore(TaskMain, "TaskMain", 10000, NULL, 1, &hTaskMain, 1);
        xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 20000, NULL, 2, &hTaskWiFi, 0);
        Serial.println("tasks created");
    }
}

void loop() {}
