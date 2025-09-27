#ifndef _MENU_H_
#define _MENU_H_

#include "functions.h"
#include "global.h"
#pragma region ________________________________ Constants

const char *mode_names[NUM_MODES]  = {
   "Domination",
   "Domination Pro",
   "Bomb mode",
   "Control Point"
};

const uint8_t greetingLength = 4;

const char *strGreeting[greetingLength]  = {
    // экран приветствия
    //01234567890123456789
    "DOMINATION PRO",
    "by Intelarms",
    "WIRELESS PROP",
    "2024"
};

const uint8_t gameModeChoosingPageSizeH = 5; // кол-во режимов, которые помещаються на одной странице
const uint8_t paramChoosingPageSize = 5;

#pragma endregion Constants


void showGreeting(uint8_t view_sec = 3) {
  //----------------------------------------------------------------------------+
  //                     Отрисовка экрана приветствия                           |
  //  [in] view_sec - время индикации экрана в с. По умолчанию 3 с.             |
  //----------------------------------------------------------------------------+
  clearScreen();
  for (uint8_t r = 0; r < greetingLength; r++)
  {
	printTFTText(strGreeting[r], 0, r*HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
    /*lcd.setCursor(0, r);
    lcd.print(strGreeting[r]);*/
  }
  tone(BUZZER_PIN, 3000, 100);
  vTaskDelay(pdMS_TO_TICKS(150));
  tone(BUZZER_PIN, 3000, 100);
  if (view_sec > 0)
    vTaskDelay(pdMS_TO_TICKS(view_sec * 1000L - 350L));
}


int8_t setGameMode(int8_t mode) {
    // Выбор режима
    // Возвращает номер режима или -1, если режим не выбрали
    /*
     01234567890123456789
    +--------------------+
    |>Domination         |
    | Domination Pro     |
    | Bomb mode          |
    | Control Point      |
    +--------------------+
    */
    static uint8_t st = 0;
    static uint8_t cur;
    static uint8_t page;
	static uint16_t textColor;

    switch (st) {
        case 0:
            cur = mode;
            st++;
            break;

        case 1:            // отрисовка страницы режимов
			clearScreen();
            page = cur / gameModeChoosingPageSizeH;
            for (uint8_t row = 0; row < gameModeChoosingPageSizeH; row++) {
                if ((page*gameModeChoosingPageSizeH + row) < NUM_MODES)
                {
                    //lcd.setCursor(1, row);
                    //lcd.print(mode_names[page*LCD_ROWS + row]);
					if (row == cur % gameModeChoosingPageSizeH)
						// выбраный режим
						textColor = CHOOSEN_TEXT_COLOR;
					else
						// остальные режими
						textColor = DEFAULT_TEXT_COLOR;

					tft.setTextColor(textColor);

					printTFTText(mode_names[page*gameModeChoosingPageSizeH + row], 0, HEADER_SPACE_H+row*STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
                }
				tft.setTextColor(DEFAULT_TEXT_COLOR);
            }
            // draw cursor in current position
            //lcd.setCursor(0, cur % LCD_ROWS);
            //lcd.write('>');
            st++;
            break;

        case 2:            // выбор режима
            char key = kpd.getKey();
            if ('A' == key || 'B' == key || 'D' == key) {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);

                if ('D' == key) {                               // D - select
                    st = 0;
                    return cur;
                }
                // clear cursor in current position
                //lcd.setCursor(0, cur % LCD_ROWS);
                //lcd.write(' ');

                // change current position
                if ('A' == key)                                 // A - up
                    cur = cur ? cur - 1 : NUM_MODES - 1;
                else                                            // B - dw
                    cur = cur < NUM_MODES - 1? cur + 1 : 0;

                /*if (page == cur / gameModeChoosingPageSizeH)
                {
                    // page not change - draw cursor in new position on the
                    lcd.setCursor(0, cur % LCD_ROWS);
                    lcd.write('>');
                }
                else*/
                    st--;                                       // change page
            }
            break;
    }
    return -1;
}

/*
void redrawValueParameter(String s, uint8_t max_digits, uint8_t pos) {
    lcd.setCursor(0, 1);
    lcd.print(s);
    for (uint8_t i = s.length(); i < max_digits; i++)
        lcd.write(' ');
    lcd.setCursor(pos, 1);
}*/

void renderParameterView(Parameter *par, String value) {

	static char type = par->getUnit();

  clearScreen();
	printTFTText(par->getName(), 0, 0, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
	if (type == 's') {
		printTFTText("seconds", DISPLAY_WIDTH-getTextWidth("seconds", STRING_FONT), HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
	}
	else if (type == 'm') {
		printTFTText("minutes", DISPLAY_WIDTH-getTextWidth("minutes", STRING_FONT), HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
	}

	printTFTText(value, 0, HEADER_SPACE_H, NOT_CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);

	if (par->getType() != 'm')
		printTFTText("[D] - exit", NO_X, DISPLAY_HEIGHT-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
}

// Надо сделать вместо функции сверху рендер изображения параметров


bool editIntParameter(Parameter *par) {
    /*----------------------------------------------------------------------------+
    // Редактирование параметра целого типа                                       |
    // par - указатель на редактируемый параметр                                  |
    // Возвращает: true-редактирование/просмотр завершен                          |
    //----------------------------------------------------------------------------+
     01234567890123456789
    +--------------------+
    |Game time           |
    |ХХХ minutes         |
    |                    |
    |     [D] - exit     |
    +--------------------+
    */
    static uint8_t st = 0;
    static uint8_t max_chars;       // max число символов в значении параметра
    static char u;                  // единицы измерения параметра
    static String inputString = "";
    static uint8_t index;
    char key;

    switch (st) {
        case 0:
            //lcd.clear();
            // имя параметра
            //lcd.print(par->getName());

            // мах длина значения параметра
            max_chars = par->getMaxLengtn();

            // единицы измерения параметра
            //lcd.setCursor(max_chars + 1, 1);
            //u = par->getUnit();
            //if (u == 's')
                //lcd.print(F("seconds"));
            //else if (u == 'm')
                //lcd.print(F("minutes"));

            //lcd.setCursor(5, 3);
            //lcd.print(F("[D] - exit"));

            // значение параметра
            //inputString = String(par->getIntValue());
            index = 0;
            //redrawValueParameter(inputString, max_chars, index);
			renderParameterView(par, inputString);

            //lcd.blink();
            st++;
            break;

        case 1:
            key = kpd.getKey();
            if (key == NO_KEY) break;
            if (key >= '0' && key <= '9')  {
                par->changed = true;
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if (index == 0)
                    inputString = key;
                else if (inputString.length() < max_chars)            // если не превысили допустимую длину параметра в символах
                    inputString += key;

                if (++index >= max_chars)
                    index = 0;

                //redrawValueParameter(inputString, max_chars, index);
				renderParameterView(par, inputString);

            }
            else if (key == 'D') {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if (par->changed) {
                    // проверка на допустимый диапазон
                    par->setValue(inputString.toInt());
                    par->isValidRange();
                }
                //lcd.noBlink();
                st = 0;
                return true;
            }
            break;
    }
    return false;
}


bool editStrParameter(Parameter *par) {
    /*----------------------------------------------------------------------------+
    // Редактирование параметра строкового типа                                   |
    // par - указатель на редактируемый параметр                                  |
    // Возвращает: true-редактирование/просмотр завершен                          |
    //----------------------------------------------------------------------------+
     01234567890123456789
    +--------------------+
    |Password            |
    |01234567            |
    |                    |
    |     D -> exit      |
    +--------------------+
    */
    static uint8_t st = 0;
    static uint8_t max_chars;       // max число символов в значении параметра
    static String inputString;
    static uint8_t index;
    char key;
    String name_par;

    switch (st) {
        case 0:                 // отрисовка имени, значения параметра, строки подсказки
            //lcd.clear();
            //lcd.print(par->getName());
            //max_chars = par->getMaxLengtn();
            //inputString = par->getStringValue();

            //lcd.setCursor(0, 3);
            //lcd.print(F("      D -> exit     "));

            // значение параметра
            index = 0;
            //redrawValueParameter(inputString, max_chars, index);
            renderParameterView(par, inputString);
            //lcd.blink();
            st++;
            break;

        case 1:                     // редактирование
            key = kpd.getKey();
            if (key == 'D') {                  // выход из редактирования
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if (par->changed) {
                    char buf[MAX_PARAMETER_VALUE_STRING_SIZE];
                    inputString.toCharArray(buf, max_chars+1);
                    par->setValue(buf);
                }
                //lcd.noBlink();
                st = 0;
                return true;
            }

            if (key >= '0' && key <= '9') {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                par->changed = true;
                inputString[index] = key;
                //lcd.write(inputString[index]);
                if (++index == max_chars)
                {
                    index = 0;
					inputString = String(" ", max_chars);
                    //lcd.setCursor(index,1);
                }
            }
            break;
    }
    return false;
}


bool editMACParameter(Parameter *par) {
    /*----------------------------------------------------------------------------+
    // Редактирование параметра МАС адреса                                        |
    // par - указатель на редактируемый параметр                                  |
    // Возвращает: true-редактирование/просмотр завершен                          |
    //----------------------------------------------------------------------------+
     01234567890123456789
    +--------------------+
    |RED team MAC        |
    |ХХ:ХX:ХX:ХX:ХX:ХX   |
    |                    |
    |C -> edit  D -> exit|
    +--------------------+
    */
    static uint8_t st = 0;
    static uint8_t max_chars;       // max число символов в значении параметра
    static String inputString;
    static uint8_t index;
    char key;

    switch (st) {
        case 0:                 // отрисовка имени, значения параметра, строки подсказки
            //lcd.clear();
            //lcd.print(par->getName());
            max_chars = par->getMaxLengtn();
            inputString = par->getStringValue();
            st++;
            break;

        case 1:                 // отрисовка имени, значения параметра, строки подсказки
            // строка подсказки
            //lcd.setCursor(0, 3);
            //lcd.print(F("C -> edit  D -> exit"));
            printTFTText("C -> edit  D -> exit", NO_X, DISPLAY_HEIGHT-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            index = 0;
            //redrawValueParameter(inputString, max_chars, index);
			renderParameterView(par, inputString);
            st++;
            break;

        case 2:                     // выход из редактирования или вход в редактирование
            key = kpd.getKey();
            if (key == 'C') {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                //lcd.setCursor(0, 3);
				renderParameterView(par, inputString);
                //lcd.print(F("* -> E        # -> F"));
				printTFTText("* -> E        # -> F", NO_X, DISPLAY_HEIGHT-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                //lcd.setCursor(index, 1);
                //lcd.blink();
                st++;
            }
            else if (key == 'D')  {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if (par->changed) {
                    char buf[MAX_PARAMETER_VALUE_STRING_SIZE];
                    inputString.toCharArray(buf, max_chars+1);
                    par->setValue(buf);
                }
                //lcd.noBlink();
                st = 0;
                return true;
            }
            break;

        case 3:             // редактирование
            key = kpd.getKey();
            if (key != NO_KEY) {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                par->changed = true;
                if ('*' == key)
                    inputString[index] = 'E';
                else if ('#' == key)
                    inputString[index] = 'F';
                else
                    inputString[index] = key;
                //lcd.write(inputString[index]);

                index++;
                if (':' == inputString[index]) {
                    index++;
                    //lcd.setCursor(index, 1);
                }

				renderParameterView(par, inputString);

                if (index == max_chars)
                {
                    //lcd.noBlink();
                    st = 1;
                }
            }
            break;
    }
    return false;
}


void drawIntValueParameter(uint32_t ival, uint8_t row) {
    //----------------------------------------------------------------------------+
    //   Отрисовка значения параметра в заданной строке                           |
    //  ival - значение параметра                                                 |
    //  row   - номер строки                                                      |
    //----------------------------------------------------------------------------+
    lcd.setCursor(0, row);
    // стираем предыдущее значение
    lcd.print("   ");
    // switch (unit) {
    //     case 's':
    //         lcd.print(F("seconds"));
    //         break;
    //     case 'm':
    //         lcd.print(F("minutes"));
    //         break;
    // }
    lcd.setCursor(0, row);
    lcd.print(ival);
}


int8_t EditParams(ListParameter* params) {
    /*----------------------------------------------------------------------------+
    // Редактирование параметров выбранного режима игры                           |
    // Возвращает: 0-редактирование не завершено; 1-параметры изменены; 2-просмотр|
    //----------------------------------------------------------------------------+
     01234567890123456789 01234567890123456789
    +--------------------+--------------------+
    |>Game_time          |>RED_team_MAC       |
    | Activated_time     | BLUE_team_MAC      |
    | Bomb_time          |                    |
    | Password           |                    |
    +--------------------+--------------------+
    */
    static uint8_t st = 0;
    static uint8_t NUMS;                                    // число параметров в режиме
    static uint8_t cur;                                     // номер текущего параметра (0..NUMS-1)
    static uint8_t page;                                    // номер страницы параметров (0..NUMS-1/4)

    char key;
    // static char sval[MAX_PARAMETER_VALUE_STRING_SIZE];      // значение параметра строкового типа
    static Parameter *par;

    switch (st) {
        case 0:
            NUMS = params->Count;
            cur = 0;
            st++;
            break;

        case 1:                           // отрисовка страницы имен параметров
            //lcd.clear();
            clearScreen();
            page = cur / paramChoosingPageSize;
            for (uint8_t row = 0; row < LCD_ROWS; row++) {
                if ((page*paramChoosingPageSize + row) < NUMS) {
                    //lcd.setCursor(1, row);
                    //lcd.print(params->parameters[page*LCD_ROWS + row]->getName());
                    printTFTText(params->parameters[page*paramChoosingPageSize + row]->getName(), NO_X, row*STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
                }
            }

            // draw cursor in current position
            //lcd.setCursor(0, cur % LCD_ROWS);
            //lcd.write('>');
            st++;
            break;

        case 2:                             // навигация по параметрам
            key = kpd.getKey();
            if (key >= 'A' && key <= 'D') {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                if ('C' == key) {                               // выход из меню параметров
                    st = 0;
                    for (cur = 0; cur < NUMS; cur++) {
                        if (params->parameters[cur]->changed)
                            return 1;
                    }
                    return 2;
                }

                if ('D' == key) {                               // переход к редактированию текущего параметра
                    par = params->parameters[cur];
                    st++;
                    break;
                }

                // clear cursor in current position
                //lcd.setCursor(0, cur % LCD_ROWS);
                //lcd.write(' ');

                // change current position
                if ('A' == key)                                 // A - up
                    cur = cur ? cur - 1 : NUMS - 1;
                else                                            // B - dw
                    cur = cur < NUMS - 1? cur + 1 : 0;

                if (page != cur / paramChoosingPageSize) {
                    // page not change - draw cursor in new position on the
                    //lcd.setCursor(0, cur % LCD_ROWS);
                    //lcd.write('>');
                    st--;                                       // change page
                }
                //else
                    //st--;                                       // change page
            }
            break;

        case 3:             // редактирование значения параметра
            switch (par->getType()) {
                case 'i':
                    st = 4;
                    break;
                case 's':
                    st = 5;
                    break;
                case 'm':
                    st = 6;
                    break;
            }
            break;

        case 4:     // редактирование значения параметра целого типа
            if (editIntParameter(par))
                st = 1;
            break;

        case 5:     // редактирование значения параметра строкового типа
            if (editStrParameter(par))
                st = 1;
            break;

        case 6:     // редактирование значения MAC адреса
            if (editMACParameter(par))
                st = 1;
            break;



  }
  return 0;
}


uint8_t dialogYesNo(String question) {
  static uint8_t st = 0;
  if (st == 0)
  {
    clearScreen();
    //lcd.clear();
    //lcd.setCursor(0, 1);
 //         //"01234567890123456789"
    //lcd.print(question);
    //lcd.setCursor(0, 3);
    //lcd.print("[*] - NO   [#] - YES");
	printTFTText(question, NO_X, DISPLAY_HEIGHT/2-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
	printTFTText("[*] - NO   [#] - YES", NO_X, DISPLAY_HEIGHT/2+HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);

    st++;
  }
  else
  {
    key = kpd.getKey();
    if (key == '*' || key == '#')
    {
      tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
      st = 0;
      return key == '*' ? DLG_NO : DLG_YES;
    }
  }
  return DLG_NONE;
}


void showMsg(String line1, String line2, uint32_t tm = 2000) {
  //lcd.clear();
  clearScreen();
  //lcd.print(line1);
  printTFTText("Message", NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
  printTFTText(line1, NO_X, HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
  //lcd.setCursor(0, 2);
  //lcd.print(line2);
  printTFTText(line2, NO_X, HEADER_SPACE_H+STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
  vTaskDelay(pdMS_TO_TICKS(tm));
}


bool startWiFi() {
  // Подключить WiFi, дождаться уведомления об успешном создании WiFi AP
  static uint8_t st = 0;
  BaseType_t rc;
  uint32_t rv;

  if (0 == st)
  {
    xTaskNotifyGive(hTaskWiFi);                       // выдать уведомление на подключение WiFi
    st++;
  }
  else
  {
    rc = xTaskNotifyWait(0,NTF_START_WIFI,&rv,0);   // ждем уведомления об успешном создании WiFi
    if (rc == pdTRUE)
    {
      log_i("WiFi создана");
      st = 0;
      return true;
    }
  }
  return false;
}


bool pressAnyKey() {
    /* ожидание нажатия кнопки "#"" для старта игры
     01234567890123456789
    +--------------------+
    |  Press # to start  |
    |                    |
    | RED: no connect    |
    |BLUE: player fault  |
    +--------------------+*/
    static uint8_t st = 0;
    static uint32_t tm = 0;
    static int8_t point;        // 0 - RED; 1 - BLUE
    static bool start;          // нажата кнопка старта игры
    static bool fEmpty = true;
    espnow_msg_t outMsg;

    switch (st) {
        case 0:
            //lcd.clear();
            clearScreen();
            //lcd.setCursor(2, 0);
            //lcd.print(F("Press # to start"));
            printTFTText("Press # to start", NO_X, STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            //lcd.setCursor(0, 2);
            //lcd.print(F(" RED:"));
            printTFTText(" RED:", 0, HEADER_SPACE_H+STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
            //lcd.setCursor(0, 3);
            //lcd.print(F("BLUE:"));
            printTFTText(" BLUE:", 0, HEADER_SPACE_H+STRING_SPACE_H+STRING_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);

            start = false;
            st++;
            break;

        case 1:                                           // ждем сообщения WiFi подключена
            if (startWiFi()) {
                //lcd.setCursor(0, 1);
                //lcd.print("TX power: ");
                int a = WiFi.getTxPower();
                //lcd.print(a);
				printTFTText("TX power: "+(String)a, NO_X, 0, CENTER_BY_X, NOT_CENTER_BY_Y, STRING_FONT);
                point = MAX_POINTS;      // текущая точка для зондирования
                st++;
            }
            break;

        case 2:                                           // формируем сообщение для проверки отклика точки
            if (start)
                st++;
            else if (xTaskGetTickCount() - tm > 500 && fEmpty) {
                outMsg.cmd = PING;
                point = point < MAX_POINTS - 1 ? point + 1 : 0;
                outMsg.data[0] = point;
                q_out_msg.push(&outMsg);
                tm = xTaskGetTickCount();
            }
            break;

        case 3:             // формирование сообщения
            if (fEmpty) {
                outMsg.cmd = PLAY_TRACK;
                outMsg.data[0] = BROADCAST;
                outMsg.data[1] = MP3_MISSION_BEGIN_60S;
                q_out_msg.push(&outMsg);

                tm = xTaskGetTickCount();
                st++;
            }
            break;

        case 4:             // ждем 4 с, пока проиграет трек
            if (xTaskGetTickCount() - tm > 5000) {
                outMsg.cmd = PLAY_TRACK;
                outMsg.data[0] = BROADCAST;
                outMsg.data[1] = MP3_CLOCK_1M;
                q_out_msg.push(&outMsg);
                st++;
            }
            break;

        case 5:                                 // передача сообщения
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;
    }

    if (st > 1) {
        key = kpd.getKey();
        if (key == '#' && !start) {
            if (G_arPeerStatus[0] == PLAYER_READY && G_arPeerStatus[1] == PLAYER_READY) {
            // if (G_arPeerStatus[0] == PLAYER_READY) {
                tone(BUZZER_PIN, BUZZER_BUTTON, BUZZER_DURATION);
                start = true;
            }
        }
    }

    fEmpty = sendESP_NOW();
    return false;
}


bool delayForStart() {
    /* задержка перед стартом игры
     01234567890123456789
    +--------------------+
    | Left seconds to go |
    |         00         |
    |                    |
    |                    |
    +--------------------+*/
    static uint8_t st = 0;
    static uint32_t secs = 0;
    static bool fEmpty = true;
    espnow_msg_t outMsg;

    switch (st) {
        case 0:                                         // чтобы не создавать новый, используем таймер игры для задержки автозапуска
            //lcd.clear();
            clearScreen();
            //lcd.print(F(" Left seconds to go"));
            printTFTText(" Left seconds to go", NO_X, DISPLAY_HEIGHT/2-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
            game_timer.SetTime(DELAY_START);
            game_timer.Start();
            st++;
            break;

        case 1:                                         // ждем окончания задержки
            game_timer.Tick();
            if (!game_timer.GetTime()) {                // Если таймер отработал
                game_timer.Stop();
                //lcd.setCursor(9, 1);
                //lcd.print("00");
				printTFTText("00", NO_X, DISPLAY_HEIGHT/2, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                st++;
            }
            else if (game_timer.Secs() != secs) {
                //lcd.setCursor(9, 1);
                secs = game_timer.Secs();
                /*if (secs < 10)
                    lcd.write('0');
                lcd.print(secs);*/
            	printTFTText(" Left seconds to go", NO_X, DISPLAY_HEIGHT/2-HEADER_SPACE_H, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
				printTFTText(secs, NO_X, DISPLAY_HEIGHT/2, CENTER_BY_X, NOT_CENTER_BY_Y, HEADER_FONT);
                tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
            }
            break;

        case 2:                                         // поместить сообщение о проигрывании трека старта игры в очередь
            outMsg.cmd = PLAY_TRACK;
            outMsg.data[0] = BROADCAST;
            outMsg.data[1] = MP3_START_GAME;
            if (!q_out_msg.push(&outMsg))
                log_e("Error: q_out_msg is full");
            st++;
            break;

        case 3:                                         // ждать завершения передачи
            if (fEmpty) {
                st = 0;
                return true;
            }
            break;
    }
    fEmpty = sendESP_NOW();                             // передача сообщения
    return false;
}


void buildParameterList(modes mode, ListParameter* params) {
    // формирование настроек для выбранного режима игры
    // title, unit, max len, value, lo, hi)
    params->addIntParameter("Game time", 'm', 3, 10, 1, 998);
    switch (mode) {
        case DOMIN:
            params->addIntParameter("Activated time", 's', 2, 4, 4, 20);
            break;
        case DOMIN_PRO:
            params->addIntParameter("Activated time", 's', 2, 4, 4, 20);
            params->addStringParameter("Password", 'n', 8, "12345678");
            break;
        case BOMB:
            params->addIntParameter("Activated time", 's', 2, 4, 4, 20);
            params->addIntParameter("Bomb time", 'm', 3, 5, 1, 997);
            params->addStringParameter("Password", 'n', 8, "12345678");
            break;
        case CTRL_POINT:
            params->addIntParameter("Repeat time", 's', 3, 5, 5, 999);
            break;
    }
    params->addStringParameter("RED team MAC", 'n', 17, "00:00:00:00:00:00");
    params->addStringParameter("BLUE team MAC", 'n', 17, "00:00:00:00:00:00");
    params->addStringParameter("LED strip MAC", 'n', 17, "00:00:00:00:00:00");
}


void ParamsFromMemoToPlay(ListParameter* params) {
    //----------------------------------------------------------------------------+
    // Пересчет параметров для выбранного режима игры                             |
    // В целях экономии EEPROM, параметры хранятся в более крупных единицах:      |
    //  - время игры в минутах, - время активации в секундах.                     |
    // Перед началом игры необходимо вызвать эту функцию для пересчета как        |
    // минимум этих параметров в мс.                                              |
    //----------------------------------------------------------------------------+
    String s;
    String name_par;
    for (uint8_t i = 0; i < params->Count; i++) {
        name_par = params->parameters[i]->getName();

        if (name_par == "Game time")
            G_u32GameTimeMS = params->getIntParameter("Game time") * 60000UL;
        else if (name_par == "Activated time")
            G_u32ActivationTimeMS = params->getIntParameter("Activated time") * 1000UL;
        else if (name_par == "Repeat time")
            G_u32RepeatTimeMS = params->getIntParameter("Repeat time") * 1000UL;
        else if (name_par == "Bomb time")
            G_u32BombTimeMS = params->getIntParameter("Bomb time") * 60000UL;
        else if (name_par == "Password")
            G_sPassword = params->getStringParameter("Password");
        else if (name_par == "RED team MAC") {
            s = params->getStringParameter("RED team MAC");
            MacStringToByteArray(s, G_aru8MACs[0]);
        }
        else if (name_par == "BLUE team MAC") {
            s = params->getStringParameter("BLUE team MAC");
            MacStringToByteArray(s, G_aru8MACs[1]);
        }
		else if (name_par == "LED strip MAC") {
			s = params->getStringParameter("LED strip MAC");
			MacStringToByteArray(s, G_aru8MACs[2]);
		}
    }
}

#endif
