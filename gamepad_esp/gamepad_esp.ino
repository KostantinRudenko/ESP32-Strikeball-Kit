/*=====================================
//           Игровой пульт
// V01 17-apr-2024
//====================================*/

#pragma region ________________________________ Constants

const uint8_t MAX_POINTS = 3; //2  // Мах количество точек
const uint8_t BROADCAST = MAX_POINTS;
const uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// String strMAColon[MAX_POINTS] = {
//    "b0:a7:32:30:48:8c",                    // МАС адрес базы красых в формате строки "B0:A7:32:2A:BA:10" (ESP32, black, 30 pin)
//    "e4:65:b8:0b:46:ec"                     // МАС адрес базы синих в формате строки "B0:A7:32:2A:BA:10" (ESP32, pink, 38 pin)
// };

#pragma endregion Constants


#pragma region ________________________________ Includes

#include <JC_Button.h>
#include <cppQueue.h>               // https://github.com/SMFSW/Queue/tree/master
#include <TFT_eSPI.h>
#include "esp_log.h"

#define	IMPLEMENTATION	FIFO

#include "global.h"
#include "params.h"
#include "macs.h"
#include "tracks.h"
#include "functions.h"
#include "menu.h"

#pragma endregion Includes


#pragma region ________________________________ Variables

esp_now_peer_info_t peerInfo;
ListParameter param_list("my-app");


#pragma endregion Variables


#pragma region ________________________________ Main_task

void TaskMain(void *pvParameters) {
    int8_t tmp;
    static team_t winner;

    for (;;) {
        switch (G_u8DeviceState) {
            case ST_GREET:                                              // приветствие
                log_i("NUM_MODES = %d", NUM_MODES);
                showGreeting();
                G_u8DeviceState++;
                break;

            case ST_CHECKPARS:
                // форматирование всех настроек
                buildParameterList(&param_list);
                log_i("buildParameterList OK !");
                if (!param_list.load()) {
                    showMsg("Incorrect parameters", " Editing required");
                    while (1);
                }

            case ST_GAMEMODE:                                           // выбор режима игры
                tmp = setGameMode(G_u8GameMode);
                if (tmp != EDIT_PARAMS)
                {
                    G_u8GameMode = (modes) tmp;
                    G_u8DeviceState = ST_PRESSANYKEY;
                }
                else {
                    G_u8DeviceState = ST_OLDPARS;
                }
                //break;

                //case ST_CHECKPARS:                                          // контроль корректности настроек


                // загрузка настроек из Flash
                //if (param_list.load())
                    //G_u8DeviceState = ST_OLDPARS;                           // запрос на игру с параметрами предыдущего сеанса
                //else {
                    // Если не корректны - установка значений по умолчанию, вывод сообщения
                    // и переход на редактирование
                    //showMsg("Incorrect parameters", " Editing required");
                    //while (1);
                //}
                break;

            case ST_OLDPARS:                                            // запрос на игру с параметрами предыдущего сеанса
                tmp = dialogYesNo(" USED OLD SETTINGS? ");
                if (DLG_NO == tmp) {
                    G_u8DeviceState = ST_EDIT_PARS;
                }
                else if (DLG_YES == tmp) {
                    G_u8DeviceState = ST_GAMEMODE;
                }
                break;

            case ST_EDIT_PARS:                                          // редактирование параметров
                G_u8DeviceState += EditParams(&param_list);
                break;

            case ST_SAVEPARS:                                           // запрос на сохранение параметров во Flash
                tmp = dialogYesNo("  SAVE INTO FLASH ? ");
                if (DLG_NONE != tmp)
                {
                    if (DLG_YES == tmp) param_list.store();
                    /*if (G_u8GameMode == EDIT_PARAMS)*/
                    G_u8DeviceState = ST_GAMEMODE;
                    //else G_u8DeviceState = ST_PARS2PLAY;
                }
                break;

            case ST_PARS2PLAY:                                          // пересчет параметров в формат для игры
                ParamsFromMemoToPlay(&param_list);
                G_u8DeviceState++;
                break;

            case ST_PRESSANYKEY:                                        // ждать нажатия кнопки "#" для старта игры
                // if (pressAnyKey(G_u8GameMode)) G_u8DeviceState += (G_u8GameMode + 1);
                if (pressAnyKey()) G_u8DeviceState++;
                break;

            case ST_DELAY_START:                                        // задержка перед стартом игры (отложенный старт)
                if (delayForStart()) G_u8DeviceState += (G_u8GameMode + 1);
                break;
            case ST_DOMIN:
            case ST_DOMIN_PRO:
                if (Domination(&param_list, &winner)) G_u8DeviceState = ST_RESULT_SCREEN;
                break;

            case ST_BOMB:
                if (Bomb(&param_list, &winner)) G_u8DeviceState = ST_RESULT_SCREEN;
                break;

            case ST_CTRL_POINT:
                if (ControlPoint(&param_list, &winner)) G_u8DeviceState = ST_RESULT_SCREEN;
                break;

            case ST_RESULT_SCREEN:                                      // Вывод результата игры
                if (GameOver(winner))
                {
                    G_u8DeviceState = 0;  //ST_GREET
                    ESP.restart();
                }
                break;
        }
    }
}


void loop() {}

#pragma endregion Main_task


#pragma region ________________________________ WiFi_task

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    espnow_event_t evt;
    evt.id_cb = ESPNOW_SEND_CB;
    evt.status = status;
    if (xQueueSend(queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        log_w("Send queue fail");
    }
}


void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    espnow_event_t evt;

    if (mac == NULL || incomingData == NULL || len <= 0) {
        log_e("Receive cb arg error");
        return;
    }

    evt.id_cb = ESPNOW_RECV_CB;
    // evt.status = 1;

    memcpy(&evt.msg, incomingData, sizeof(espnow_msg_t));

    log_i("evt.msg.cmd = %d", evt.msg.cmd);

    // помещаем элемент в конец очереди и не ждем, если очередь переполнена
    if (xQueueSend(queue, &evt, 0) != pdPASS)
    {
        log_i("Receive queue overflow");
    }
}


bool connectToWiFi() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        log_e("Error initializing ESP-NOW");
        return false;
    }

    if (esp_now_register_send_cb(OnDataSent) != ESP_OK) {
        log_e("Failed esp_now_register_send_cb");
        return false;
    }

    peerInfo.channel = 1;
    peerInfo.encrypt = false;


    // memcpy(peerInfo.peer_addr, broadcastMAC, 6);

    // // Add broadcast peer
    // if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    //     log_e("Failed to add peer with broadcastMAC");
    //     return false;
    // }

    // register peers
    for (int8_t peer = 0; peer <= MAX_POINTS; peer++) {
        // Register peer
        // Последним в массиве МАС адресов ДОЛЖЕН БЫТЬ broadcast !!!
        memcpy(peerInfo.peer_addr, G_aru8MACs[peer], 6);
        // Add peer
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            log_e("Failed to add peer %d", peer);
            return false;
        }
    }

    // Register for a callback function that will be called when data is received
    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        log_e("Failed esp_now_register_recv_cb");
        return false;
    }
    return true;
}


void TaskWiFi(void *pvParameters) {
    static uint8_t st = 0;
    uint32_t rv;
    BaseType_t rc;

    espnow_event_t evt;
    //bool xLink = LED_OFF;                 // состояние светодиода LINK
    //uint32_t u32LinkLEDTime;
    uint32_t u32AckWaitTimer;               // таймер ожидания ответного сообщения

    // EventBits_t uxBits;
    // const TickType_t xTicksToWait = pdMS_TO_TICKS(100);

    disableCore0WDT();

    for (;;)
    {
        switch (st)
        {
            case 0:                          // Отключена
                // ждем уведомление "Подключить WiFi"
                rv = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (rv)
                {
                    if (connectToWiFi())
                    {
                        // rc = xTaskNotify(hTaskMain,NTF_START_WIFI,eSetBits);
                        xTaskNotify(hTaskMain, NTF_START_WIFI, eSetBits);
                        st++;
                    }
                    else
                    {
                        xTaskNotify(hTaskMain, NTF_ERROR_WIFI, eSetBits);
                        st = 5;
                    }
                }
                break;


            case 1:      // Ожидание уведомления от главной задачи

                // ждем событие передать сообщение или отключить WIFI
                // Значение слово события возвращается в rv, которое затем проверяется и в зависимости от того, какой
                // установленного бита(ов), мы выполняем команду.
                // esp_err_t result;
                rc = xTaskNotifyWait(0, NTF_STOP_WIFI | NTF_SEND_WIFI, &rv, 0);
                // rc = xTaskNotifyWait(0, NTF_STOP_WIFI | NTF_SEND_WIFI, &rv, portMAX_DELAY);
                if (rc == pdTRUE) {
                    if (rv & NTF_STOP_WIFI)
                        st = 4;

                    if (rv & NTF_SEND_WIFI) {
                        // recv_evt_evt.msg
                        // Send message via ESP-NOW
                        // Передаем сообщение. Последним в массиве МАС адресов ДОЛЖЕН БЫТЬ broadcast !!!
                        if (esp_now_send(G_aru8MACs[send_evt.msg.data[0]], (uint8_t *) &send_evt.msg, sizeof(espnow_msg_t)) == ESP_OK) {
                            send_evt.status |= MSG_PUT_SEND_CB;
                            st++;
                        }
                        else
                            xTaskNotify(hTaskMain, NTF_SEND_FINAL, eSetBits);
                    }
                }
                break;

            case 2:       // ожидание ответного сообщения
                if (xQueueReceive(queue, &evt, 0) == pdTRUE) {
                    switch (evt.id_cb) {
                        case ESPNOW_SEND_CB:
                            if (evt.status == ESP_NOW_SEND_SUCCESS) {
                                log_i("ESP_NOW_SEND_SUCCESS");
                                send_evt.status |= MSG_SEND_OK;
                                // if (evt.msg.cmd == PING)
                                if (send_evt.msg.cmd == PING)
                                    // Запускаем таймер ожидания ответного сообщения
                                    u32AckWaitTimer = xTaskGetTickCount();
                                else
                                    st++;
                            }
                            else {
                                log_i("ESP_NOW SEND FAIL");
                                xTaskNotify(hTaskMain, NTF_SEND_FINAL, eSetBits);
                                st = 1;
                            }
                            break;

                        case ESPNOW_RECV_CB:
                            recv_evt.msg = evt.msg;
                            send_evt.status |= MSG_RECV_OK;
                            log_i("RECV ACK MSG SUCCESS");
                            st++;
                            break;
                    }
                }
                else if (send_evt.status & MSG_SEND_OK && (xTaskGetTickCount() - u32AckWaitTimer > 2000)) {
                    xTaskNotify(hTaskMain, NTF_SEND_FINAL, eSetBits);
                    st = 1;
                }
                break;

            case 3:
                xTaskNotify(hTaskMain, NTF_SEND_FINAL, eSetBits);
                st = 1;
                break;

            case 4:  // процедура отключения WiFi
                WiFi.disconnect(true);  // Disconnect from the network
                WiFi.mode(WIFI_OFF);    // Switch WiFi off
                vTaskDelay(pdMS_TO_TICKS(500));
                rc = xTaskNotify(hTaskMain, NTF_STOP_WIFI, eSetBits);
                st = 0;
                break;

            case 5:  // Ошибка подключения WiFi
                break;
    }
  }
}






/*
void TaskWiFi(void *pvParameters) {
  static bool espnow_init = false;
  uint32_t rv;
  BaseType_t rc;
  BaseType_t s;
  msg_esp_now_t qitem;


  ed_espnow_event_t evt;
  bool is_broadcast = false;


  // EventBits_t uxBits;
  // const TickType_t xTicksToWait = pdMS_TO_TICKS(100);

  disableCore0WDT();

  for (;;)
  {
    if (!espnow_init)
    {
      // ждем уведомление "Подключить WiFi"
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      if (connectToWiFi())
      {
        xTaskNotify(hTaskMain, NTF_START_WIFI, eSetBits);
        espnow_init = true;
      }
      else
      {
        xTaskNotify(hTaskMain, NTF_ERROR_WIFI, eSetBits);
      }
    }
    else
    {
      while (xQueueReceive(queue, &evt, portMAX_DELAY) == pdTRUE)
      {
        switch (evt.id)
        {
          case ED_ESPNOW_SEND_CB:
            ed_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
            is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);
            break;

        }
      }


      // Если в очереди появилось сообщение для передачи. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      // s = xQueueReceive(queue_out, &qitem, 0);
      s = xQueueReceive(queue, &qitem, pdMS_TO_TICKS(500));
      if (s == pdPASS)
      {
        esp_now_send(G_aru8MACs[qitem.data[0]], (uint8_t *) &qitem, sizeof(msg_esp_now_t));
      }
    }
  }
}
*/

#pragma endregion WiFi_task


#pragma region ________________________________ Setup_function

void setup(void) {

    log_i("Gamepad start.");

// uint32_t sec;
// int8_t t;

// sec = 320; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 300; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 280; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 60; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 30; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 10; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);

// sec = 8; t = getTimeMarker(sec);
// log_i("Seconds = %d;  index = %d", sec, t);


// while(1);

// mySketchPrefs.begin("myPrefs", true);
// size_t whatsLeft = freeEntries();    // this method works regardless of the mode in which the namespace is opened.
// Serial.printf("There are: %u entries available in the namespace table.\n, whatsLeft);
// mySketchPrefs.end();


preferences.begin("my-app", true);






    //lcd.init();
    //lcd.backlight();

    //TFT_eSPI tft = TFT_eSPI();

    //Serial.begin(9600);
    //Serial.println();
    tft.init();
    tft.setRotation(3);

    clearScreen();

    ONE_DIGIT_WIDTH = getTextWidth("0", STRING_FONT);

    // Allow allocation of all timers
    ESP32PWM::allocateTimer(0);
        // ESP32PWM::allocateTimer(1);
        // ESP32PWM::allocateTimer(2);
        // ESP32PWM::allocateTimer(3);

    ledcSetup(BUZZER_PWM_CHANNEL, 1000, 8);
    tone(BUZZER_PIN, 10, 50);
    delay(100);

    //kpd.setDebounceTime(100);
    // kpd.setHoldTime(500);
    // индикация МАС адреса этого устройства
    if (kpd.getKey() != NO_KEY)
    {
        printTFTText(WiFi.macAddress(), 0, 20, true, false, HEADER_FONT);
        //lcd.print(WiFi.macAddress());
        delay(60000);
    }

    blueButton.begin();
    redButton.begin();
    blueButton.read();
    redButton.read();

    /*
    byte bar1[8] = {
        B10000,
        B10000,
        B10000,
        B10000,
        B10000,
        B10000,
        B10000,
        B10000,
    };
    byte bar2[8] = {
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
    };
    byte bar3[8] = {
        B11100,
        B11100,
        B11100,
        B11100,
        B11100,
        B11100,
        B11100,
        B11100,
    };
    byte bar4[8] = {
        B11110,
        B11110,
        B11110,
        B11110,
        B11110,
        B11110,
        B11110,
        B11110,
    };
    byte bar5[8] = {
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
    };

    lcd.createChar(0, bar1);
    lcd.createChar(1, bar2);
    lcd.createChar(2, bar3);
    lcd.createChar(3, bar4);
    lcd.createChar(4, bar5);
    */


/*
    param_list.addIntParameter("Game_time", 10);
    param_list.addIntParameter("Activated_time", 4);
    param_list.addIntParameter("Bomb_time", 5);
    param_list.addStringParameter("Password", "12345678");

    bool success = param_list.load(); // should return false, as we didn't store the file yet


    while (1);
*/



    // Используйте собственное пространство имён (не больше 15 символов), чтобы избежать коллизий
    // false - read/write mode; true - open or create in read-only mode.
    // preferences.begin("my-app", false);
    // // Gr_u8PointCount = readPointCountFlash();  // Чтение количества точек
    // readMACsFlash();                          // Чтение МАС адресов точек
    // readPreferences();                        // Чтение параметров игры


    // Когда вы больше не нуждаетесь в работе с настройками, то не забывайте вызывать функцию end().
    // Закрываем Preferences
    // preferences.end();

    queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (queue == NULL)
    {
        log_e("Create queue fail");
        printTFTText("Create queue fail", 0, 20, true, false, STRING_FONT);
        //lcd.print(F("Create queue fail"));
        while (1) {;}
        // return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(TaskMain, "TaskMain", 20000, NULL, 1, &hTaskMain, 1);
    xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 20000, NULL, 2, &hTaskWiFi, 0);
}

#pragma endregion Setup_function
