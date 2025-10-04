#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include "global.h"
#include "getTimeHMS.h"
#include <csignal>

#pragma region ________________________________ Constants

const int16_t LCD_H_DOTS = 100;                // число точек LCD по горизонтали  = число точек на символ * число символов (5*20=100)
const char *team_names[3] = {
    "    ",
    " RED",
    "BLUE"
};

// enum bomb_t {BS_IDLE = 0, BS_RUN, BS_STOP};

const uint32_t arTimeMikers[4] = {10, 30, 60, 300};
const uint32_t arBombTimeMikers[2] = {300000, 480000};

#pragma endregion Constants


#pragma region ________________________________ Variables

uint8_t G_u8Team;                             // активная команда
TimerExt game_timer(false);                   // таймер обратного счета времени игры
TimerExt bomb_timer(false);                   // таймер обратного счета времени бомбы
TimerExt timerRed(true);                      // таймер прямого счета команды красных
TimerExt timerBlue(true);                     // таймер прямого счета команды синих

#pragma endregion Variables

void printTFTText(String text, uint16_t x, uint16_t y, bool centerByX, bool CenterByY, const String font){
    tft.loadFont(font);

    if (centerByX) {
        x = (TFT_WIDTH - tft.textWidth(text)) / 2;
    }
    if (CenterByY) {
        y = (TFT_HEIGHT - tft.fontHeight()) / 2;
    }

	tft.drawString(text, x, y);

	tft.unloadFont();
}

uint16_t getTextWidth(String s, const String font) {
	tft.loadFont(font);
	uint16_t width = tft.textWidth(s);
	tft.unloadFont();
	return width;
}

void clearScreen() {
	tft.fillScreen(TFT_BLACK);
}

void clearSpace(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
    tft.fillRect(x, y, width, height, color);
}


void display3DigitsInt(uint8_t col, uint8_t row, uint16_t var, char zero = ' ') {
    //----------------------------------------------------------------------------+
    //               Вывод двухзначного числа с лидирующим нулем                  |
    //  [in]  col  - колонка первого символа                                      |
    //  [in]  row  - строка первого символа                                       |
    //  [in]  var  - число (0..999)                                               |
    //  [in]  zero - '0'- лидирующиq нуль                                       |
    //----------------------------------------------------------------------------+
    lcd.setCursor(col, row);
    if (var < 100)
        lcd.write(zero);
    else if (var < 10)
        lcd.write(zero);
    lcd.print(var);
}

uint8_t ProcessButton(const Button button, uint8_t *progress, uint32_t *time) {
    if (button.isReleased()) {
        *progress = 0;
        *time = xTaskGetTickCount();
        return 0;
    }

    if (*progress >= LCD_H_DOTS) return 100;
    if (*progress == 0) {
        lcd.setCursor(0, 1);
        lcd.print(F("                    "));
    }

    uint32_t var = (xTaskGetTickCount() - *time);
    uint8_t new_progress = map(var, 0, G_u32ActivationTimeMS, 0, LCD_H_DOTS);

    while (*progress < new_progress) {

		if (*progress % 10 == 0) {
			espnow_msg_t msg;
			msg.cmd = LIGHT_STRIP;
			msg.data[0] = G_u8Team;
			msg.data[1] = *process;
			sendESP_NOW_ToMAC(G_aru8MACs[2], &msg);
		}

        lcd.setCursor(*progress / 5, 1);
        lcd.print((char)(*progress % 5));
        tone(BUZZER_PIN, *progress * 25);
        *progress += 1;
    }

    if (*progress >= LCD_H_DOTS) {
        // *progress = LCD_H_DOTS;
        lcd.setCursor(0, 1);
        lcd.print(F("                   "));
        return 100;
    }
    return *progress * 100UL / LCD_H_DOTS;
}


void parseMessage(espnow_event_t* send_event, espnow_msg_t* recv_msg) {
    if (send_event->msg.cmd == PING) {
        uint8_t point = send_event->msg.data[0];
        G_arPeerStatus[point] = PEER_NO_CONNECT;        // Упреждающая установка. При успешном приеме будет перезаписана
        if (send_event->status & MSG_RECV_OK) {
            log_i("сообщение принято");
            if (point == recv_msg->data[0])
                G_arPeerStatus[point] = recv_msg->data[1];      // если сообщение принято
        }
        // отрисовка состояния точки
        lcd.setCursor(6, 2 + point);
        lcd.print(sPointNameStates[G_arPeerStatus[point]]);
    }
}


bool sendESP_NOW() {
    //----------------------------------------------------------------------------+
    //                  Передать сообщение через ESPNOW                           |
    //  return true, если передача завершена, иначе false                         |
    //----------------------------------------------------------------------------+
    static uint8_t st = 0;
    uint32_t rv;

    if (st == 0) {
        if (q_out_msg.getCount() == 0) return true;

        q_out_msg.pop(&send_evt.msg);
        send_evt.status = MSG_RDY_TO_SEND;
        xTaskNotify(hTaskWiFi, NTF_SEND_WIFI, eSetBits);
        st++;
    }
    else if (xTaskNotifyWait(0, NTF_SEND_FINAL, &rv, 0) == pdTRUE) {
        send_evt.status |= MSG_SEND_FINAL;
        parseMessage(&send_evt, &recv_evt.msg);
        st = 0;
    }  // pdFALSE - если тайм-аут
    return false;
}

bool sendESP_NOW_ToMAC(const uint8_t *mac_addr, espnow_msg_t *msg) {
    //----------------------------------------------------------------------------+
    //         Передать сообщение через ESP-NOW на указанный MAC-адрес            |
    //  mac_addr - целевой MAC-адрес (6 байт)                                     |
    //  msg - указатель на структуру сообщения espnow_msg_t                       |
    //  return true, если передача завершена, иначе false                         |
    //----------------------------------------------------------------------------+
    static uint8_t st = 0;
    uint32_t rv;

    if (st == 0) {
        if (msg == NULL || mac_addr == NULL) {
            log_e("Invalid MAC address or message");
            return false;
        }

        send_evt.msg = *msg; // Копируем сообщение
        send_evt.status = MSG_RDY_TO_SEND;
        if (esp_now_send(mac_addr, (uint8_t *) &send_evt.msg, sizeof(espnow_msg_t)) == ESP_OK) {
            send_evt.status |= MSG_PUT_SEND_CB;
            xTaskNotify(hTaskWiFi, NTF_SEND_WIFI, eSetBits);
            st++;
        } else {
            log_e("Failed to send ESP-NOW message");
            return false;
        }
    }
    else if (xTaskNotifyWait(0, NTF_SEND_FINAL, &rv, 0) == pdTRUE) {
        send_evt.status |= MSG_SEND_FINAL;
        parseMessage(&send_evt, &recv_evt.msg);
        st = 0;
        return true;
    }
    return false;
}

void RenderStaticView() {
	clearSpace(0, 0, DISPLAY_WIDTH, HEADER_SPACE_H, TFT_BLACK);
	// убрать на clearSpace
    switch (G_u8GameMode) {
        case DOMIN:
			printTFTText("DOMINATION", NO_X, SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            /*
            lcd.print(F("     DOMINATION     "));
            lcd.setCursor(0, 1);
            lcd.print(F("                    "));
            lcd.setCursor(6, 1);
            showTimeHMS(lcd, game_timer.Secs());

            lcd.setCursor(5, 2);
            lcd.print(F("     "));
            lcd.setCursor(5, 3);
            lcd.print(F("     "));
            */

            if (G_u8Team != NOONE)
            {
                //lcd.setCursor(5, 1 + G_u8Team);
                //lcd.print(F("====>"));
                printTFTText("====>", NO_X, HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            }
            break;

        case DOMIN_PRO:
			printTFTText("DOMINATION PRO", NO_X, SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            break;

        case BOMB:
        case CTRL_POINT:
            /*
            Строка 0:
            "BOMB MODE" - нет авторизации и нет активации/дезактивации
            "Enter PASS> ********" - идет авторизация
            "ARMING" - идет активация заряда
            "DISARMING" - идет дезактивация заряда

            Строка 1:
            "GAME TIME   00:15:20" - нет активации/дезактивации
            "====================" - идет активация/дезактивация заряда (progress bar)

            Строка 2:
            "" - IDLE, STOP заряд не активирован или остановленнет активации/дезактивации
            "====================" - идет активация/дезактивация заряда (progress bar)

            Строка 3:
            "" - заряд не был активирован IDLE
            "BOMB TIME   00:01:59" - RUN-STOP-END-BOOM
            "====================" - идет активация/дезактивация заряда (progress bar)
            */
            if (G_u8GameMode == BOMB)
				printTFTText("BOMB MODE", NO_X, SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                //lcd.print(F("      BOMB MODE     "));
            else
				printTFTText("CONTROL POINT", NO_X, SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                //lcd.print(F("   CONTROL POINT    "));
			printTFTText("Game time: ", 0, HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
            //lcd.setCursor(0, 1);
            //lcd.print(F("Game time:  "));
            //lcd.setCursor(12, 1);
            //showTimeHMS(lcd, game_timer.Secs());
            printTFTText(String(getTimeHMS(game_timer.Secs())), getTextWidth("Game time: ", STRING_FONT), HEADER_SPACE_H+STRING_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
            break;
    }
}


int8_t getTimeMarker(uint32_t secs) {
    for (int8_t i = 4; i > 0; i--)
        if (secs >= arTimeMikers[i-1]) return i;
    return 0;
}


bool enterPassword(bool *fCursorChange) {
    const uint8_t max_chars = 8;
    const uint8_t start_pos = 20 - max_chars;
    static uint8_t st = 0;
    static uint16_t phraseWidth = getTextWidth("Enter PASS> ", HEADER_FONT);
	static uint16_t underscoreWidth = getTextWidth("_", HEADER_FONT);
    static String inputString = "        ";
    static uint8_t index;
    static uint32_t tm;
    char key;

    switch (st) {
        case 0:                 // ожидание нажатия любой кнопки - отрисовка приглашения
            if (kpd.getKey() != NO_KEY) {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                //lcd.setCursor(0, 0);
                //lcd.print(F("Enter PASS> ********"));
				uint8_t labelStartPosition = (DISPLAY_WIDTH - phraseWidth - underscoreWidth*max_chars)/2;
				printTFTText("Enter PASS> ", labelStartPosition, 0, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
				for (uint8_t i = 0; i < max_chars; i++) {
					printTFTText("_", phraseWidth+(underscoreWidth+12)*i, 0, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
				}
                inputString = "        ";
                index = 0;
                //lcd.setCursor(start_pos + index, 0);
                //lcd.blink();
                st++;
            }
            break;

        case 1:                     // редактирование
            if (*fCursorChange) {
                //lcd.setCursor(start_pos + index, 0);
                *fCursorChange = false;
            }
            key = kpd.getKey();
            if (key >= '0' && key <= '9') {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if (key == G_sPassword[index]) {
                    inputString[index] = key;
                    //lcd.write(inputString[index]);
					printTFTText((String)key, phraseWidth+(underscoreWidth+12)*index, 0, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    if (++index == max_chars)
                    {
                        //lcd.noBlink();
                        //lcd.setCursor(0,0);
                        //lcd.print(F("    Password OK !   "));
						clearSpace(0, 0, DISPLAY_WIDTH, HEADER_HEIGHT, TFT_BLACK);
						printTFTText("Password OK !", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                        tm = xTaskGetTickCount();
                        st++;
                    }
                }
            }
            break;

        case 2:                     // 2 сек показываем сообщение "Password OK !"
            if (xTaskGetTickCount() - tm >= 2000) {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                st = 0;
                return true;
            }
            break;
    }
    return false;
}


bool Domination(ListParameter* params, team_t* winner) {
    //----------------------------------------------------------------------------+
    // строка 0: Название игры                                                    |
    // строка 1: таймер обратного отсчёта времени игры.                           |
    // нижней строке таймеры времени доминирования для каждой команды.            |
    // Побеждает команда, чей таймер показывает большее время доминирования.      |
    //----------------------------------------------------------------------------+
    /*
     01234567890123456789 01234567890123456789
    +--------------------+--------------------+
    |     DOMINATION     |       АRMING       |
    |      00:00:00      |**************      |
    | RED ====>  00:00:00| RED        00:00:00|
    |BLUE        00:00:00|BLUE        00:00:00|
    +--------------------+--------------------+*/
    static uint8_t st = 0;
    static uint32_t secs = game_timer.Secs();
	static uint16_t teamTimerPositionX = DISPLAY_WIDTH-getTextWidth("00:00:00", HEADER_FONT);
	static uint16_t redTimerPositionY = HEADER_SPACE_H+STRING_SPACE_H;
	static uint16_t blueTimerPositionY = HEADER_SPACE_H+STRING_SPACE_H+HEADER_SPACE_H;
    static uint32_t time_press_red = xTaskGetTickCount();
    static uint32_t time_press_blue = xTaskGetTickCount();
    static uint8_t progressRed = 0;
    static uint8_t progressBlue = 0;
    uint8_t redValue, blueValue;
    static uint32_t prev; // пред. значения таймера команды, владеющей точкой
    static uint8_t point; // 0,1
    static bool fEmpty = true;

    static int8_t i8CheckTimeCount;
    espnow_msg_t outMsg;

    redButton.read();
    blueButton.read();
    redValue = ProcessButton(redButton, &progressRed, &time_press_red);
    blueValue = ProcessButton(blueButton, &progressBlue, &time_press_blue);

    switch (st)
    {
        case 0:
            //lcd.clear();
            clearScreen();
            // digitalWrite(RED_LED_PIN, LED_OFF);
            // digitalWrite(BLUE_LED_PIN, LED_OFF);
            G_u8Team = NOONE;
            //lcd.print(F("     DOMINATION"));
            printTFTText("DOMINATION", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            game_timer.SetTime(G_u32GameTimeMS);  // устанавливаем таймер игры
            secs = game_timer.Secs();

            i8CheckTimeCount = getTimeMarker(secs);

            //lcd.setCursor(6, 1);
            //showTimeHMS(lcd, secs);
			printTFTText(getTimeHMS(secs), NO_X, HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);

            // названия команд
            //lcd.setCursor(0, 2);
            //lcd.print(team_names[RED]);
				printTFTText(team_names[RED], 0, HEADER_SPACE_H+STRING_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            //lcd.setCursor(0, 3);
            //lcd.print(team_names[BLUE]);
				printTFTText(team_names[BLUE], 0, HEADER_SPACE_H*2+STRING_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);


            // таймеры команд
            //lcd.setCursor(12, 2);
            //showTimeHMS(lcd, timerRed.Secs());
			printTFTText(getTimeHMS(timerRed.Secs()), teamTimerPositionX, redTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            //lcd.setCursor(12, 3);
            //showTimeHMS(lcd, timerBlue.Secs());
			printTFTText(getTimeHMS(timerBlue.Secs()), teamTimerPositionX, blueTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);

            game_timer.Start();                   // запускаем таймер игры
            st++;
            break;

        case 1:    // отрисовка команды-владельца gamepad
            RenderStaticView();
            st++;
            break;

        case 2:   // статика - отображаем время
            if (redValue || blueValue) {
                //lcd.setCursor(0, 0);
				clearSpace(0, 0, DISPLAY_WIDTH, HEADER_SPACE_H, TFT_BLACK);
                if (G_u8Team == NOONE) {
                    //lcd.print(F("        ARMING      "));
					printTFTText("ARMING", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_CAP_POINT;
                }
                else {
                    //lcd.print(F("      DISARMING     "));
					printTFTText("DISARMING", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    // Проигрываем на базе команды, которая не деактивирует точку
                    outMsg.data[0] = G_u8Team - 1;
                    outMsg.data[1] = MP3_CAP_OUR_POINT;
                }
                outMsg.cmd = PLAY_TRACK;
                if (!q_out_msg.push(&outMsg))
                    log_e("Error: q_out_msg is full");
                st++;
            }
            break;

        case 3:                                       // идет захват/освобождение устройства
            if (redValue >= 100 || blueValue >= 100) {
                if (G_u8Team == NOONE) {
                    if (redValue >= 100) {
                        timerRed.Start();
                        timerBlue.Stop();
                        G_u8Team = RED;
                    }
                    else if (blueValue >= 100) {
                        timerBlue.Start();
                        timerRed.Stop();
                        G_u8Team = BLUE;
                    }

                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = G_u8Team - 1;
                    outMsg.data[1] = MP3_WE_CAP_POINT;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");

                    outMsg.data[0] = G_u8Team == RED ? BLUE - 1: RED - 1;
                    outMsg.data[1] = MP3_ENEMY_CAP_POINT;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                }
                else {
                    timerRed.Stop();
                    timerBlue.Stop();
                    outMsg.data[0] = G_u8Team - 1;
                    outMsg.data[1] = MP3_WE_LOST_POINT;
                    outMsg.cmd = PLAY_TRACK;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    G_u8Team = NOONE;
                }

                RenderStaticView();
                st++; // точка захвачена/освобождена
            }
            else if (!redValue && !blueValue) // отпуcтили кнопку раньше ArmTime
                st = 1;
            break;

        case 4:  // ждем отжатия обоих кнопок
            if (!redValue && !blueValue)
                st = 1;
            break;

        case 5:  // время игры истекло
            if (timerRed.GetTime() == timerBlue.GetTime())
                *winner = NOONE;
            else
                *winner = timerRed.GetTime() > timerBlue.GetTime() ? RED : BLUE;
            st++;

        case 6:
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;
    } // switch

    fEmpty = sendESP_NOW();

    if (st < 5) {
        game_timer.Tick();
        if (!game_timer.GetTime()) {  // Если время игры истекло
            timerRed.Stop();
            timerBlue.Stop();
            game_timer.Stop();
            //lcd.setCursor(12, 2);
            //showTimeHMS(lcd, timerRed.Secs());
			printTFTText(getTimeHMS(timerRed.Secs()), teamTimerPositionX, redTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            //lcd.setCursor(12, 3);
            //showTimeHMS(lcd, timerBlue.Secs());
			printTFTText(getTimeHMS(timerBlue.Secs()), teamTimerPositionX, blueTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            st = 5;
        }
        else if (st != 3) {
            if (game_timer.Secs() != secs) {
                //lcd.setCursor(6, 1);
				clearSpace(0, HEADER_SPACE_H, DISPLAY_WIDTH, STRING_SPACE_H, TFT_BLACK);
                secs = game_timer.Secs();
                showTimeHMS(lcd, secs);
                tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
            }

            if (G_u8Team == RED) {
                timerRed.Tick();
                if (timerRed.Secs() != prev) {
                    //lcd.setCursor(12, 2);
					clearSpace(teamTimerPositionX, redTimerPositionY, DISPLAY_WIDTH-teamTimerPositionX, HEADER_SPACE_H, TFT_BLACK);
                    prev = timerRed.Secs();
                    //showTimeHMS(lcd, timerRed.Secs());
					printTFTText(getTimeHMS(timerRed.Secs()), teamTimerPositionX, redTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                }
            }
            else if (G_u8Team == BLUE) {
                timerBlue.Tick();
                if (timerBlue.Secs() != prev) {
                    prev = timerBlue.Secs();
                    //lcd.setCursor(12, 3);
					clearSpace(teamTimerPositionX, blueTimerPositionY, DISPLAY_WIDTH-teamTimerPositionX, HEADER_SPACE_H, TFT_BLACK);
                    //showTimeHMS(lcd, timerBlue.Secs());
					printTFTText(getTimeHMS(timerBlue.Secs()), teamTimerPositionX, blueTimerPositionY, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);

                }
            }

            if (i8CheckTimeCount) {
                if (secs <= arTimeMikers[i8CheckTimeCount - 1]) {
                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_MISSION_END_10S + i8CheckTimeCount - 1;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    i8CheckTimeCount--;
                }
            }
        }
    }
    return false;
}


bool Bomb(ListParameter* params, team_t* winner) {
    //----------------------------------------------------------------------------+
    // При старте таймер игры остановлен. При нажатии любой кнопки на клавиатуре, |
    // на дисплее появляется приглашение для ввода пароля. Игрок вводит пароль и  |
    // если он верный, разрешается активация заряда нажатием любой из кнопок.     |
    // После активации запускается таймер игры и таймер времени заряда.|
    // Также высвечивается команда, активировавшая заряд. Заряд можно деактивиро- |
    // вать, но потребуется ввод пароля. Игра продолжается до обнуления таймера   |
    // заряда или таймера игры или деактивации заряда. Побеждпет команда,         |
    // обнулившая таймер заряда или деактивировшая его.                           |
    //----------------------------------------------------------------------------+
    /*
     01234567890123456789 01234567890123456789 01234567890123456789
    +--------------------+--------------------+--------------------+--------------------+
    |      BOMB MODE     |Enter PASS> ********|        ARMING      |      BOMB MODE     |
    |Game time:  00:15:20|Game time:  00:15:20|***************     |Game time:  00:15:20|
    |                    |   BLUE activated   |                    |   BLUE activated   |
    |                    |Bomb time:  00:01:59|                    |Bomb time:  00:01:59|
    +--------------------+--------------------+--------------------+--------------------+*/
    static uint8_t st = 0;
    static uint32_t secs = 0;
    static uint32_t time_press_red = xTaskGetTickCount();
    static uint32_t time_press_blue = xTaskGetTickCount();
    static uint32_t start_bomb_time;
    static uint8_t progressRed = 0;
    static uint8_t progressBlue = 0;
    uint8_t redValue, blueValue;

    static bool fEmpty = true;
    static int8_t i8CheckTimeCount;             // счетчик временных треков до окончания игры
    static int8_t i8CheckBombTimeCount;         // счетчик временных треков от момента активации заряда
    static bool fCursorChange = true;
    espnow_msg_t outMsg;
    static bool activated = false;
    static bool pass_ok;                          // true-авторизация выполнена

	static uint16_t bombTimePhraseLength = getTextWidth("Bomb time: ", HEADER_FONT);
	static uint16_t timerPositionX = DISPLAY_WIDTH-getTextWidth("00:00:00", HEADER_FONT);

    redButton.read();
    blueButton.read();

    // if (pass_ok && (st > 2 && st < 6)) {
    if (pass_ok && (st > 0 && st < 6)) {
        if (G_u8Team != RED)
            redValue = ProcessButton(redButton, &progressRed, &time_press_red);
        // else
        //     redValue = 0;

        if (G_u8Team != BLUE)
            blueValue = ProcessButton(blueButton, &progressBlue, &time_press_blue);
        // else
        //     blueValue = 0;
    }

    switch (st) {
        case 0:                                   // инициализация переменных, отрисовка статической информации на дисплее
            clearScreen();
            G_u8Team = NOONE;
            pass_ok = false;
            *winner = NOONE;
            i8CheckTimeCount = getTimeMarker(secs);
            //lcd.clear();
            game_timer.SetTime(G_u32GameTimeMS);  // устанавливаем таймер игры
            // secs = 0;
            secs = game_timer.Secs();
            i8CheckTimeCount = getTimeMarker(secs);
            bomb_timer.SetTime(G_u32BombTimeMS);  // устанавливаем таймер бомбы

            // счетчик временных треков от момента активации заряда
            if (G_u32BombTimeMS < 300000)
                i8CheckBombTimeCount = 0;
            else if (G_u32BombTimeMS < 480000)
                i8CheckBombTimeCount = 1;
            else
                i8CheckBombTimeCount = 2;

            activated = false;
            st++;
            break;

        case 1:    // отрисовка команды-владельца gamepad
            RenderStaticView();
            if (*winner == NOONE)
                st++;
            else
                st = 7;
            break;

        case 2:    // ожидание авторизации
            if (!pass_ok) {
                pass_ok = enterPassword(&fCursorChange);
            }
            else
                st++;
            break;

        case 3:    // ожидание старта активации/деактивации
            if (redValue || blueValue) {
                //lcd.setCursor(0, 0);
                if (G_u8Team == NOONE) {
                    //lcd.print(F("        ARMING      "));
					printTFTText("ARMING", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    // Проигрываем на базе команды, которая не захватывает точку
                    outMsg.data[0] = redValue ? BLUE - 1 : RED - 1;
                    outMsg.data[1] = MP3_THEY_ACTIVATE_BOMB;
                    outMsg.cmd = PLAY_TRACK;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    }
                else {
                    //lcd.print(F("      DISARMING     "));
					printTFTText("DISARMING", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                }
                st++;
            }
            break;

        case 4:                                             // ожидание завершения активации/деактивации
            if (redValue >= 100 || blueValue >= 100) {      // если активация/деактивация завершена
                if (G_u8Team == NOONE) {                    // если активация завершена
                    G_u8Team = redValue >= 100 ? RED : BLUE;
                    //lcd.setCursor(3, 2);
                    //lcd.print(team_names[G_u8Team]);
                    //lcd.print(F(" activated"));
					printTFTText((String)team_names[G_u8Team]+" activated", NO_X, HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    // Проигрываем на базе команды, которая не активировала бомву
                    outMsg.data[0] = G_u8Team == RED ? BLUE - 1 : RED - 1;
                    outMsg.data[1] = MP3_STOP_BOMB;
                    game_timer.Start();                 // запускаем таймер игры
                    //lcd.setCursor(0, 3);
                    //lcd.print(F("Bomb time:  "));
					printTFTText("Bomb time:  ", 0, HEADER_SPACE_H*3, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                    bomb_timer.Start();
                    start_bomb_time = xTaskGetTickCount();
                    activated = true;
                }
                else {                                          // если деактивация завершена
                    bomb_timer.Stop();
                    activated = false;
                    //lcd.setCursor(0, 2);
                    //lcd.print("                    ");
					clearSpace(0, HEADER_SPACE_H*2, DISPLAY_WIDTH, HEADER_SPACE_H, TFT_BLACK);
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_DEACTIVATED_BOMB;
                    *winner = G_u8Team == RED ? BLUE : RED;  // запоминаем победителя
                    G_u8Team = NOONE;
                }

                outMsg.cmd = PLAY_TRACK;
                if (!q_out_msg.push(&outMsg))
                    log_e("Error: q_out_msg is full");

                // RenderStaticView();
                st++;                                           // устройство активировано/деактивировано
            }
            else if (!redValue && !blueValue) {                  // отпуcтили кнопку раньше ArmTime
                // redValue = ProcessButton(redButton, &progressRed, &time_press_red);
                // blueValue = ProcessButton(blueButton, &progressBlue, &time_press_blue);
                noTone(BUZZER_PIN);
                st = 1;                                         // авторизация сохраняется
            }
            break;

        case 5:  // ждем отжатия обоих кнопок
            if (!redValue && !blueValue) {
                pass_ok = false;        // для последующих активации/деактивации снова потребуется ввод пароля
                st = 1;
            }
            break;

        case 6:  // если время игры истекло или таймер заряда обнулился
            if (bomb_timer.GetTime() == 0 && activated && G_u8Team != NOONE)
                *winner = (team_t) G_u8Team;
            else
                *winner = NOONE;
            st++;
            break;

        case 7:     // ожидание завершения передачи сообщений
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;

    } // switch

    fEmpty = sendESP_NOW();

    if (game_timer.isRunning()) {
        game_timer.Tick();
        // if (u8BombState == BS_RUN)
        if (activated)
            bomb_timer.Tick();

        // Если время игры истекло или таймер заряда обнулился
        if (!game_timer.GetTime() || !bomb_timer.GetTime()) {
            game_timer.Stop();
            bomb_timer.Stop();
            //lcd.setCursor(12, 1);
			clearSpace(timerPositionX, HEADER_SPACE_H, DISPLAY_WIDTH-timerPositionX, HEADER_SPACE_H, TFT_BLACK);
            //showTimeHMS(lcd, 0);
			printTFTText((String)getTimeHMS(0), timerPositionX, HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            //lcd.setCursor(12, 3);
			clearSpace(timerPositionX, HEADER_SPACE_H*3, DISPLAY_WIDTH-timerPositionX, HEADER_SPACE_H, TFT_BLACK);
            //showTimeHMS(lcd, bomb_timer.Secs());
			printTFTText((String)getTimeHMS(bomb_timer.Secs()), timerPositionX, HEADER_SPACE_H*3, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            st = 6;
        }
        else if (st != 4) {
            if (game_timer.Secs() != secs) {
                //lcd.setCursor(12, 1);
				clearSpace(timerPositionX, HEADER_SPACE_H, DISPLAY_WIDTH-timerPositionX, HEADER_SPACE_H, TFT_BLACK);
                secs = game_timer.Secs();
                //showTimeHMS(lcd, secs);
				printTFTText((String)getTimeHMS(secs), timerPositionX, HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);

                // if (u8BombState == BS_RUN) {
                if (activated) {
                    //lcd.setCursor(12, 3);
					clearSpace(timerPositionX, HEADER_SPACE_H*3, DISPLAY_WIDTH-timerPositionX, HEADER_SPACE_H, TFT_BLACK);
                    //showTimeHMS(lcd, bomb_timer.Secs());
					printTFTText((String)getTimeHMS(bomb_timer.Secs()), timerPositionX, HEADER_SPACE_H*3, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                }

                tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
                fCursorChange = true;
            }

            if (i8CheckTimeCount) {
                if (secs <= arTimeMikers[i8CheckTimeCount - 1]) {
                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_MISSION_END_10S + i8CheckTimeCount - 1;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    i8CheckTimeCount--;
                }
            }

            if (i8CheckBombTimeCount) {
                if (xTaskGetTickCount() - start_bomb_time <= arBombTimeMikers[i8CheckBombTimeCount - 1]) {
                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_STOP_BOMB_3M + i8CheckBombTimeCount - 1;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    i8CheckBombTimeCount--;
                }
            }

        }
    }
    return false;
}


// bool Bomb(ListParameter* params, team_t* winner) {
    //----------------------------------------------------------------------------+
    // При старте игры таймер игры остановлен и дисплей погашен.                  |
    // При первом нажатии кнопки на клавиатуре, дисплей включается и на экране    |
    // появляется приглашение для ввода пароля. Игрок вводит пароль и если он     |
    // верный - разрешается активация заряда нажатием любой из кнопок. После      |
    // активации запускается таймер игры и таймер отсчета времени до конца заряда.|
    // Также высвечивается команда, активировавшая заряд. Заряд можно деактивиро- |
    // вать, но потребуется ввод пароля. Игра продолжается до обнуления таймера   |
    // заряда или таймера игры. Побеждпет команда, обнулившая таймер заряда.      |
    //----------------------------------------------------------------------------+
    /*
     01234567890123456789 01234567890123456789 01234567890123456789
    +--------------------+--------------------+--------------------+--------------------+
    |      BOMB MODE     |Enter PASS> ********|        ARMING      |      BOMB MODE     |
    |GAME TIME   00:15:20|GAME TIME   00:15:20|***************     |GAME TIME   00:15:04|
    |                    |   BLUE activated   |                    |   BLUE activated   |
    |                    |BOMB TIME   00:01:59|                    |BOMB TIME   00:01:59|
    +--------------------+--------------------+--------------------+--------------------+*/



    /*
    static uint8_t st = 0;
    static uint32_t secs = 0;
    static uint32_t time_press_red = xTaskGetTickCount();
    static uint32_t time_press_blue = xTaskGetTickCount();
    static uint8_t progressRed = 0;
    static uint8_t progressBlue = 0;
    uint8_t redValue, blueValue;

    static bool fEmpty = true;
    static int8_t i8CheckTimeCount;
    static bool fCursorChange = true;
    espnow_msg_t outMsg;

    // static uint8_t u8BombState;
    static bool activated = false;
    static bool pass_ok;                          // true-авторизация выполнена

    redButton.read();
    blueButton.read();

    if (pass_ok && (st > 2 && st < 6)) {
        redValue = ProcessButton(redButton, &progressRed, &time_press_red);
        blueValue = ProcessButton(blueButton, &progressBlue, &time_press_blue);
    }

    switch (st) {
        case 0:                                   // инициализация переменных, отрисовка статической информации на дисплее
            G_u8Team = NOONE;
            pass_ok = false;
            i8CheckTimeCount = getTimeMarker(secs);
            lcd.clear();
            game_timer.SetTime(G_u32GameTimeMS);  // устанавливаем таймер игры
            // secs = 0;
            secs = game_timer.Secs();
            i8CheckTimeCount = getTimeMarker(secs);
            bomb_timer.SetTime(G_u32BombTimeMS);  // устанавливаем таймер бомбы
            // u8BombState = BS_IDLE;
            activated = false;
            st++;
            break;

        case 1:    // отрисовка команды-владельца gamepad
            RenderStaticView();
            st++;
            break;

        case 2:    // ожидание авторизации
            if (!pass_ok) {
                pass_ok = enterPassword(&fCursorChange);
            }
            else
                st++;
            break;

        case 3:    // ожидание старта активации/деактивации
            if (redValue || blueValue) {
                lcd.setCursor(0, 0);
                if (G_u8Team == NOONE) {
                    lcd.print(F("        ARMING      "));
                    // Проигрываем на базе команды, которая не захватывает точку
                    outMsg.data[0] = redValue ? BLUE - 1 : RED - 1;
                    outMsg.data[1] = MP3_THEY_ACTIVATE_BOMB;
                    outMsg.cmd = PLAY_TRACK;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    }
                else {
                    lcd.print(F("      DISARMING     "));
                    // Проигрываем на базе команды, которая не деактивирует точку
                    // outMsg.data[0] = G_u8Team - 1;
                    // outMsg.data[1] = MP3_CAP_OUR_POINT;
                }

                st++;
            }
            break;

        case 4:                                             // ожидание завершения активации/деактивации
            if (redValue >= 100 || blueValue >= 100) {      // если активация/деактивация завершена
                if (G_u8Team == NOONE) {                    // если активация завершена
                    G_u8Team = redValue >= 100 ? RED : BLUE;
                    lcd.setCursor(3, 2);
                    lcd.print(team_names[G_u8Team]);
                    lcd.print(F(" activated"));
                    // Проигрываем на базе команды, которая не активировала бомву
                    outMsg.data[0] = G_u8Team == RED ? BLUE - 1 : RED - 1;
                    outMsg.data[1] = MP3_STOP_BOMB;
                    // if (u8BombState == BS_IDLE) {           // если первая активация
                        game_timer.Start();                 // запускаем таймер игры
                        lcd.setCursor(0, 3);
                        lcd.print(F("BOMB TIME   "));
                        // "BOMB TIME   00:01:59"
                    // }
                    bomb_timer.Start();
                    // u8BombState = BS_RUN;
                    activated = true;
                }
                else {                                          // если деактивация завершена
                    bomb_timer.Stop();
                    // u8BombState = BS_STOP;
                    activated = false;
                    lcd.setCursor(0, 2);
                    lcd.print("                    ");
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_DEACTIVATED_BOMB;
                    G_u8Team = NOONE;
                }

                // outMsg.cmd = PLAY_TRACK;
                // if (!q_out_msg.push(&outMsg))
                //     log_e("Error: q_out_msg is full");

                // RenderStaticView();
                st++;                                           // устройство активировано/деактивировано
            }
            else if (!redValue && !blueValue)                   // отпуcтили кнопку раньше ArmTime
                st = 1;                                         // авторизация сохраняется
            break;

        case 5:  // ждем отжатия обоих кнопок
            if (!redValue && !blueValue) {
                pass_ok = false;        // для последующих активации/деактивации снова потребуется ввод пароля
                st = 1;
            }
            break;

        case 6:  // если время игры истекло или таймер заряда обнулился
            // if (bomb_timer.GetTime() == 0 && u8BombState == BS_RUN && G_u8Team != NOONE)
            if (bomb_timer.GetTime() == 0 && activated && G_u8Team != NOONE)
                *winner = (team_t) G_u8Team;
            else
                *winner = NOONE;
            st++;
            break;

        case 7:     // ожидание завершения передачи сообщений
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;

    } // switch

    fEmpty = sendESP_NOW();

    if (game_timer.isRunning()) {
        game_timer.Tick();
        // if (u8BombState == BS_RUN)
        if (activated)
            bomb_timer.Tick();

        // Если время игры истекло или таймер заряда обнулился
        if (!game_timer.GetTime() || !bomb_timer.GetTime()) {
            game_timer.Stop();
            bomb_timer.Stop();
            lcd.setCursor(12, 1);
            showTimeHMS(lcd, 0);
            lcd.setCursor(12, 3);
            showTimeHMS(lcd, bomb_timer.Secs());
            st = 6;
        }
        else if (st != 4) {
            if (game_timer.Secs() != secs) {
                lcd.setCursor(12, 1);
                secs = game_timer.Secs();
                showTimeHMS(lcd, secs);

                // if (u8BombState == BS_RUN) {
                if (activated) {
                    lcd.setCursor(12, 3);
                    showTimeHMS(lcd, bomb_timer.Secs());
                }
                tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);

                fCursorChange = true;
            }

            if (i8CheckTimeCount) {
                if (secs <= arTimeMikers[i8CheckTimeCount - 1]) {
                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_10S_END + i8CheckTimeCount - 1;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    i8CheckTimeCount--;
                }
            }

        }
    }
    return false;
}
*/


bool ControlPoint(ListParameter* params, team_t* winner) {
    /*----------------------------------------------------------------------------+
    // Побеждает команда, которая за время игры успеет нажать свою кнопку большее |
    // количество раз. Таймер игры запускается по первому нажатию кнопки.         |
    // Параметр Repeat time (время блокировки повторного нажатия) задается в сек  |
    // от 0 до 999. Если 0 - промежуток между нажатиями будет рандомный. Если     |
    // нажатие разрешено, кнопка мигает.                                          |
    //----------------------------------------------------------------------------+
     01234567890123456789
    +--------------------+
    |    CONTROL POINT   |
    |Game time:  00:15:20|
    | RED points:     000|
    |BLUE points:     000|
    +--------------------+*/
    const uint32_t LO_REPEAT_TIME_MS = 5000;
    const uint32_t HI_REPEAT_TIME_MS = 30000;
    const uint32_t LONG_PRESS_MS = 100;
    const uint8_t led_pins[2] = {RED_LED_PIN, BLUE_LED_PIN};

    static uint8_t st = 0;
    static uint32_t secs = 0;
    static bool fEmpty = true;
    static int8_t i8CheckTimeCount;                     // счетчик временных треков до окончания игры
    espnow_msg_t outMsg;
    uint8_t i;

    static Button buttons[2] = {redButton, blueButton};
    static bool pressed[2];                             // true - кнопка нажата
    static uint32_t leaving_time[2];                    // время отжатия кнопки, мс
    static uint32_t lock_time[2];                       // время блокировки повторного нажатия кнопки, мс
    static uint16_t points[2];                          // количество нажатий кнопки
    static bool fRandomTime;                            // true - рандомное время блокировки повторного нажатия кнопки

    switch (st) {
        case 0:                                         // инициализация переменных, отрисовка статической информации на дисплее
            lcd.clear();
            lcd.setCursor(4, 0);
            lcd.print(F("CONTROL POINT"));

            game_timer.SetTime(G_u32GameTimeMS);  // устанавливаем таймер игры
            secs = game_timer.Secs();
            i8CheckTimeCount = getTimeMarker(secs);

            lcd.setCursor(0, 1);
            lcd.print(F("Game time:"));
            lcd.setCursor(12, 1);
            showTimeHMS(lcd, secs);

            lcd.setCursor(0, 2);
            lcd.print(F(" RED points:"));
            lcd.setCursor(0, 3);
            lcd.print(F("BLUE points:"));

            fRandomTime = G_u32RepeatTimeMS == 0;
            for (i = 0; i < 2; i++) {
                points[i] = 0;
                pressed[i] = false;
                leaving_time[i] = 0;
                if (fRandomTime)
                    lock_time[i] = random(LO_REPEAT_TIME_MS, HI_REPEAT_TIME_MS);
                else
                    lock_time[i] = G_u32RepeatTimeMS;
                display3DigitsInt(17, i + 2, points[i]);
                digitalWrite(led_pins[i], pressed[i] ? LED_OFF : LED_ON);
            }
            st++;
            break;

        case 1:                                   // ожидание нажатия кнопки
            for (i = 0; i < 2; i++) {
                buttons[i].read();
                if (xTaskGetTickCount() - leaving_time[i] > lock_time[i]) {
                    if (buttons[i].isPressed())
                    {
                        pressed[i] = true;
                        digitalWrite(led_pins[i], LED_OFF);
                    }

                    if (!pressed[i] && !digitalRead(led_pins[i])) {
                        digitalWrite(led_pins[i], LED_ON);
                        outMsg.cmd = PLAY_TRACK;
                        outMsg.data[0] = i;
                        outMsg.data[1] = MP3_ENABLE_CTRLPOINT;
                        if (!q_out_msg.push(&outMsg))
                            log_e("Error: q_out_msg is full");
                    }

                    else if (buttons[i].wasReleased()) {
                        if (!game_timer.isRunning())
                            game_timer.Start();
                        leaving_time[i] = xTaskGetTickCount();
                        points[i]++;
                        display3DigitsInt(17, 2 + i, points[i]);
                        if (fRandomTime)
                            lock_time[i] = random(LO_REPEAT_TIME_MS, HI_REPEAT_TIME_MS);
                        pressed[i] = false;
                    }
                }
                else
                    digitalWrite(led_pins[i], LED_OFF);
            }
            break;

        case 2:                                   // время игры истекло
            if (points[RED - 1] == points[BLUE - 1])
                *winner = NOONE;
            else
                *winner = points[RED - 1] > points[BLUE - 1] ? RED : BLUE;

            digitalWrite(RED_LED_PIN, *winner == RED ? LED_ON : LED_OFF);
            digitalWrite(BLUE_LED_PIN, *winner == BLUE ? LED_ON : LED_OFF);
            st++;
            break;

        case 3:     // ожидание завершения передачи сообщений
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;

    } // switch

    fEmpty = sendESP_NOW();

    if (game_timer.isRunning()) {
        game_timer.Tick();
        // Если время игры истекло
        if (!game_timer.GetTime()) {
            game_timer.Stop();
            lcd.setCursor(12, 1);
            showTimeHMS(lcd, 0);
            st = 2;
        }
        else if (game_timer.Secs() != secs) {
            lcd.setCursor(12, 1);
            secs = game_timer.Secs();
            showTimeHMS(lcd, secs);
            tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);

            if (i8CheckTimeCount) {
                if (secs <= arTimeMikers[i8CheckTimeCount - 1]) {
                    outMsg.cmd = PLAY_TRACK;
                    outMsg.data[0] = BROADCAST;
                    outMsg.data[1] = MP3_MISSION_END_10S + i8CheckTimeCount - 1;
                    if (!q_out_msg.push(&outMsg))
                        log_e("Error: q_out_msg is full");
                    i8CheckTimeCount--;
                }
            }
        }
    }

    return false;
}


void showWinerTeam(team_t winner) {
    // индикация победителя
    switch (G_u8GameMode) {
        case DOMIN:
            lcd.setCursor(6, 1);
            if (winner == NOONE) {
                lcd.print(F("  DRAW!     "));
            }
            else {
                lcd.print(team_names[winner]);
                lcd.print(F(" WIN!   "));
            }
            // гасим возможную стрелочку
            lcd.setCursor(5, 2);
            lcd.print(F("     "));
            lcd.setCursor(5, 3);
            lcd.print(F("     "));
            break;
        case DOMIN_PRO:
            break;
        case BOMB:
        case CTRL_POINT:
            lcd.setCursor(0, 1);
            if (winner == NOONE)
                            //01234567890123456789
                lcd.print(F("         DRAW !      "));
            else if (winner == RED)
                lcd.print(F("       RED WIN!      "));
            else
                lcd.print(F("       BLUE WIN!     "));
            break;
    }
}


bool GameOver(team_t winner) {
    /*  Индикация результата игры
     01234567890123456789 01234567890123456789
    +--------------------+--------------------+
    |     GAME OVER !    |Press # for new game|
    |     BLUE WIN !     |     BLUE WIN !     |
    |                    |                    |
    |                    |                    |
    +--------------------+--------------------+
    */
    static uint8_t st = 0;
    static uint32_t tm;
    static bool xFlash;
    uint32_t rv;
    static bool fEmpty = true;
    espnow_msg_t outMsg;

    switch (st) {
        case 0:
            xFlash = false;
            tm = 0;
            showWinerTeam(winner);
            outMsg.cmd = PLAY_TRACK;
            outMsg.data[0] = BROADCAST;
            if (winner == NOONE)
                outMsg.data[1] = MP3_NO_WIN;
            else
                outMsg.data[1] = winner == RED ? MP3_RED_WIN : MP3_BLUE_WIN;
            if (!q_out_msg.push(&outMsg))
                log_e("Error: q_out_msg is full");
            st++;
            break;

        case 1:         // ждем завершения передачи, затем отключаем WiFi
            if (fEmpty) {
                xTaskNotify(hTaskWiFi, NTF_STOP_WIFI, eSetBits);
                st++;
            }
            break;

        case 2:         // ждем уведомления об отключении WiFi
            if (xTaskNotifyWait(0, NTF_STOP_WIFI, &rv, 0) == pdTRUE) {
                log_i("WiFi отключена");
                st++;
            }
            break;

        case 3:       // мигание сообщения в строке 1 и ожидание нажатия перезапуска
            if (kpd.getKey() == '#') {
                st = 0;
                return true;
            }
            if (xTaskGetTickCount() - tm > 1500) {
                lcd.setCursor(0, 0);
                if (!xFlash)
                    lcd.print(F("     GAME OVER !    "));
                else
                    lcd.print(F("Press # for new game"));
                tm = xTaskGetTickCount();
                xFlash = !xFlash;
            }
            break;
    }

    if (st < 2) fEmpty = sendESP_NOW();

    return false;
}

#endif
