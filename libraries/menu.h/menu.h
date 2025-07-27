#ifndef _MENU_H_
#define _MENU_H_

#include "lcd_utils.h"

const uint8_t START_COL_PARAMS = 17;    // стартовая колонка параметра
const uint8_t MAX_STRING_LENGTH = 3;    // мах кол-во символов в строке значения параметра

const char *mode_names[]  = {
  "0. Demo Mode",
  "1. Conquest",
  "2. Breakthrough",
  "3. Only Me",
  "4. Battle Royale",
  "5. Sabotage",
  "6. Domination",
  "7. HardPoint",
};


bool playTrack(uint8_t track, bool force = false) {
  static uint32_t tm = 0;
  
  if (!G_xAudioConnected) return true;

  if (!force && (xTaskGetTickCount() < tm || !digitalRead(PLAYER_BUSY_PIN))) return false;

  audio.play(track);
  tm = xTaskGetTickCount() + Gc_u16BusyCheckDelayMS;
  return true;
}


void showGreeting() {
  lcd.clear();
  lcd.setCursor(5, 1);                 
  lcd.print("INTELARMS");
  lcd.setCursor(2, 2);
  lcd.print("Innovative Gaming");
  vTaskDelay(pdMS_TO_TICKS(3000));      // пауза 3 с
}


int8_t setGameMode() {
  static uint8_t st = 0;
  static uint8_t page = 0;
 
  switch (st)
  {
    case 0:
      page = 0;
      playTrack(TREK1, true);
      st++;
      break;

    case 1:            // отрисовка режимов
      lcd.clear();
      for (uint8_t row = 0; row < 4; row++)
      {
        lcd.setCursor(0, row);
        lcd.print(mode_names[page*4 + row]);
      }
      st++;
      break;

    case 2:            // выбор режима
      char key = kpd.getKey();
      if ('A' == key || 'B' == key)
      {
        _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
        page = (page + 1) & 1;
        st--;
        break;
      }
      else if((key >= '0' && key <= '3' && 0 == page) ||
        (key >= '3' && key <= '7' && 1 == page))
      {
        _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
        st = 0;
        return (key - '0');
      }
      break; 
  }
  return -1; 
}


void DrawValueParameter(uint8_t nopar, uint8_t row) {
  //----------------------------------------------------------------------------+
  //   Отрисовка значения параметра в заданной строке                           |
  //  [in] nopar - номер параметра в наборе                                     |
  //  [in] row   - номер строки                                                 |
  //----------------------------------------------------------------------------+
  lcd.setCursor(START_COL_PARAMS, row);
  lcd.print("   ");
  lcd.setCursor(START_COL_PARAMS, row);
  lcd.print(params[nopar]);
}


void SetFocusValueParameter(uint8_t nopar, uint8_t row, uint8_t len){
  //----------------------------------------------------------------------------+
  //   Отрисовка значения параметра в заданной строке и установка курсора       |
  //  [in] nopar - номер параметра в наборе                                     |
  //  [in] row   - номер строки                                                 |
  //  [global] params - текущий набор параметров                                |
  //----------------------------------------------------------------------------+
  DrawValueParameter(nopar, row);
  lcd.setCursor(START_COL_PARAMS + len, row);
}


void ChangeParameter(uint8_t nopar, uint8_t row, uint8_t *st) {
  static String inputString;
  static uint8_t index;

  if ('*' == key)           // обнулить параметр
  {
    params[nopar] = 0;
    inputString = "0";
    index = 0;
  }  
  else        // '0'..'9' изменить параметр
  {
    if (0 == *st)
    {
      inputString = String(params[nopar]);
      *st = 1;
      index = 0;
    }
    if (index < MAX_STRING_LENGTH)              // если не превысили допустимую длину параметра в символах
    {
      if (index < inputString.length())
      {
        inputString[index] = key;
      }
      else
      {
        inputString += key;  
      }
      params[nopar] = inputString.toInt();
      //IsParamValid(nopar); // ограничение параметра допустимым диапазоном
      inputString = String(params[nopar]);
      if (!('0' == key && 0 == index))
      {
        if (++index >= MAX_STRING_LENGTH)
        {
          index = 0;  
        }
      }    
    }
    else
    {
      index = 0;
    }
  }
  SetFocusValueParameter(nopar,row, index);
  // log_i("inputString.length = %d",inputString.length());
}


int8_t EditParams(int8_t mode) {
  // Редактирование параметров выбранного режима
  static uint8_t st = 0;
  static uint8_t cur_par;
  uint8_t sindshow;
  uint8_t eindshow;
  uint8_t pg;
  static uint8_t sub_st = 0;
  static uint8_t NUMS;                        // число параметров в режиме
  static uint8_t ARR_NO_PARS[PARAMS_IN_SET];  // номера параметров в режиме

  switch (st)
  {
    case 0:
      switch (mode)
      {
        case DEMO:                            // DEMO PARAMS
          NUMS = 1;
          ARR_NO_PARS[0] = P_WIFI_POWER;      // WiFi power
          break;
          
        case GO_FAST:                         // CONQUEST PARAMS
        case SUPRESSION:                      // BREAKTHROUGH PARAMS
        case FUCING_FORCE:                    // ONLY ME PARAMS
        case DOMINATION:                      // DOMINATION PARAMS        
          NUMS = 3;
          ARR_NO_PARS[0] = P_GAME_TIME;       // game_time_min
          ARR_NO_PARS[1] = P_WIFI_POWER;      // WiFi power
          ARR_NO_PARS[2] = P_ACTIV_TIME;      // activation_time_s
          break;

        case CONTROL_POINT:                   // BATTLE ROYALE PARAMS
          NUMS = 4;
          ARR_NO_PARS[0] = P_GAME_TIME;       // game_time_min
          ARR_NO_PARS[1] = P_WIFI_POWER;      // WiFi power
          ARR_NO_PARS[2] = P_ACTIV_TIME;      // activation_time_s
          ARR_NO_PARS[3] = P_PAUSE_SCAN;      // "Пауза между сканированиями одной и той же карты, мин"        
          break;

        case ADD_TIME:                        // SABOTAGE PARAMS
          NUMS = 4;
          ARR_NO_PARS[0] = P_GAME_TIME;       // game_time_min
          ARR_NO_PARS[1] = P_WIFI_POWER;      // WiFi power
          ARR_NO_PARS[2] = P_ACTIV_TIME;      // activation_time_s
          ARR_NO_PARS[3] = P_PENALTY_TIME;    // "Штрафное время для команды противника, мин"
          break;

        case HARDPOINT:                       // HARDPOINT PARAMS       
          NUMS = 4;
          ARR_NO_PARS[0] = P_GAME_TIME;       // game_time_min
          ARR_NO_PARS[1] = P_WIFI_POWER;      // WiFi power
          ARR_NO_PARS[2] = P_REPEAT_TIME;     // "Время повторного учета игрока, сек"
          ARR_NO_PARS[3] = P_POINTS;          // "Количество очков одного игрока"
          // для этого режима время активации всегда 3 сек, поєтому его не редактируем activation_time_ms = 3000;
          break;                               
      }
      cur_par = 0;
      st++;
      break;

    case 1:         // отрисовка страницы параметров
      lcd.clear();
      sindshow = cur_par & 0b11111100;
      eindshow = sindshow + 3;
      if (eindshow >= NUMS)
      {
        eindshow = NUMS - 1;
      }

      for (uint8_t row = 0; row <= (eindshow & 3); row++)
      {
        // отрисовка названия параметра
        lcd.setCursor(0, row);
        lcd.print(param_names[ARR_NO_PARS[sindshow + row]]);

        // отрисовка значения параметра
        lcd.setCursor(START_COL_PARAMS, row);
        lcd.print(params[ARR_NO_PARS[sindshow + row]]);
      }

      lcd.setCursor(START_COL_PARAMS, (cur_par & 3));
      lcd.blink();
      sub_st = 0;      
      st++;
      break;

    case 2:
      switch (key)
      {
        case 'A':  // up  по меню параметров
          if (!IsParamValid(ARR_NO_PARS[cur_par]))      // если параметр был ограничен
            DrawValueParameter(ARR_NO_PARS[cur_par], (cur_par & 3));    // перерисовка
          pg = cur_par & 0xfc;
          cur_par = cur_par > 0 ? cur_par - 1 : NUMS - 1;
          if ((cur_par & 0xfc) != pg)
            st = 1;   // на отрисовку новой страницы
          else
          {
            lcd.setCursor(START_COL_PARAMS, (cur_par & 3));
            sub_st = 0;
          }
          _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
          break;
      
        case 'B':   // down
          if (!IsParamValid(ARR_NO_PARS[cur_par]))      // если параметр был ограничен
            DrawValueParameter(ARR_NO_PARS[cur_par], (cur_par & 3));    // перерисовка
          pg = cur_par & 0xfc;
          cur_par = cur_par < NUMS-1 ? cur_par + 1 : 0;
          if ((cur_par & 0xfc) != pg)
            st = 1;   // на отрисовку новой страницы
          else
          {
            lcd.setCursor(START_COL_PARAMS, (cur_par & 3));
            sub_st = 0;
          }
          _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
          break;
      
        case 'D':   // exit
          if (IsParamValid(ARR_NO_PARS[cur_par]))      // если значение параметра корректно
          {
            st = 0;
            sub_st = 0;
            _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
            lcd.noBlink();
            return 1;    // переход в состояние запроса сохранения параметров ????
          }
          DrawValueParameter(ARR_NO_PARS[cur_par], (cur_par & 3));    // если параметр был ограничен - перерисовка
          break;

        case '*':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          ChangeParameter(ARR_NO_PARS[cur_par], (cur_par & 3), &sub_st);
          _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
          break;
      }
      break;           
  }
  return 0;
}


uint8_t dialogYesNo(String question) {
  static uint8_t st = 0;
  if (st == 0)
  {
    lcd.clear();
    lcd.setCursor(0, 1);
 //         //"01234567890123456789"                 
    lcd.print(question);
    lcd.setCursor(0, 3);
    lcd.print("[*] - NO   [#] - YES");
    st++;
  }
  else
  {
    key = kpd.getKey();
    if (key == '*' || key == '#')
    { 
      _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
      st = 0;
      return key == '*' ? DLG_NO : DLG_YES;
    }
  }
  return DLG_NONE;
}


void showStation(uint8_t col, uint8_t row, uint8_t no) {
  lcd.setCursor(col, row);
  lcd.print("   ");
  lcd.setCursor(col, row);
  lcd.print(no);
}


void getPlayerName(uint8_t team, uint8_t noplayer,char player[4]) {
  if (team == 1)
    player[0]= 'R';
  else
    player[0] = 'B';

  player[1] = '0' + noplayer / 10;
  player[2] = '0' + noplayer % 10;
  player[3] = 0;
}


void showMsg(String line1, String line2, uint32_t tm = 2000) {
  lcd.clear();                
  lcd.print(line1);
  lcd.setCursor(0, 2);
  lcd.print(line2);
  vTaskDelay(pdMS_TO_TICKS(tm));
}


/*
void keypadEvent(KeypadEvent key){
  // if (keypad.getState() == HOLD)
  // {
  //   key_hold = key;  
  // }
  // else
  //   key_hold = NO_KEY;  
  key_hold = kpd.getState() == HOLD ? key : NO_KEY;
}
*/

#endif    // _MENU_H_