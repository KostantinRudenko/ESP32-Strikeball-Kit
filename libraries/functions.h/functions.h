#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_


int8_t searchMACInArray(const uint64_t mac) {
  // возвращает -1, если МАС адрес не найден,
  // иначе 00ТР.РРРР, где:
  // Т (Team) - команда. 0-RED; 1-BLUE
  // Р.РРРР (Player) - индекс игрока от 0 до 31 (MAX_PLAYERS-1)
  for (int8_t team = 0; team < 2; team++)
  {
    uint8_t agame_players = teams[team].getGamePlayers();
    if (agame_players > 0 && agame_players <= MAX_PLAYERS)
    {
      for(int8_t player = 0; player < agame_players; player++)
      {
        if (mac == macs[team][player])
        {
          return (team == 0 ? player : player + MAX_PLAYERS);
        }
      }
    }
  }
  return -1;
}


void updatePlayerState(int8_t i8TeamPlayer, bool fConnect) {
  int8_t t = i8TeamPlayer < MAX_PLAYERS ? RED : BLUE;
  int8_t p = i8TeamPlayer & (MAX_PLAYERS - 1);
  teams[t].updatePlayerState(p,fConnect);
}


void ParamsFromMemoToPlay() {
  //----------------------------------------------------------------------------+  
  // В целях экономии EEPROM, параметры хранятся в более крупных единицах:      |
  //  - время игры в минутах, - время активации в секундах.                     |
  // Перед началом игры необходимо вызвать эту функцию для пересчета как        |
  // минимум этих параметров в мс.                                              |
  //----------------------------------------------------------------------------+
  game_time_ms = params[P_GAME_TIME] * 60000UL;
  activation_time_ms = params[P_ACTIV_TIME] * 1000UL;
  pause_before_scans_ms = params[P_PAUSE_SCAN] * 60000UL;
  penalty_time_ms = params[P_PENALTY_TIME] * 60000UL;
  reenter_time_ms = params[P_REPEAT_TIME] * 1000UL;
  
  for (uint8_t t = 0; t < 2; t++)
  {
    teams[t].clearTeamData();
    teams[t].setLakePlayer(random(teams[t].getGamePlayers()));          // номер счастливого игрока
  }

  game_timer.SetTime(game_time_ms);
  win_team = NONE;                         // команда-победитель
  whose_point = NONE;                      // команда, владеющая точкой
  first_team = NONE;                       // первая команда на точке
  log_i("RED Lake Player = %d", teams[RED].getLakePlayer());
  log_i("BLUE Lake Player = %d", teams[BLUE].getLakePlayer());
}


bool startWiFiAP() {
  // Подключить WiFi, создать АР, дождаться уведомления об успешном создании WiFi AP 
  static uint8_t st = 0;
  BaseType_t rc;
  uint32_t rv;

  if (0 == st)
  {
    xTaskNotifyGive(hTaskWiFi);                       // выдать уведомление на подключение АР
    st++;
  }
  else
  {                                           
    rc = xTaskNotifyWait(0,NTF_START_WIFIAP,&rv,0);   // ждем уведомления об успешном создании WiFi AP
    if (rc == pdTRUE)
    {
      log_i("WiFi AP создана");
      st = 0;
      return true;
    }
  }
  return false;
}


bool stopWiFiAP() {
  // Отдключить WiFi, дождаться уведомления об успешном отключении WiFi AP 
  static uint8_t st = 0;
  BaseType_t rc;
  uint32_t rv;

  if (0 == st)
  {
    rc = xTaskNotify(hTaskWiFi,NTF_STOP_WIFIAP,eSetBits);                   // выдать уведомление Отключить WiFi АР
    st++;
  }
  else
  {                                                                         // ждем сообщения WiFi AP отключена
    rc = xTaskNotifyWait(0,NTF_STOP_WIFIAP,&rv,0);
    if (rc == pdTRUE)
    {
      log_i("WiFi AP отключена");
      st = 0;
      return true;
    }
  }
  return false;
}


bool pressAnyKey() {
  /* ожидание нажатия любой кнопки для старта игры
   01234567890123456789
  ______________________
  |   Press any key    |
  |                    |
  |   for start game   |
  |                    |
  |____________________| */
  static uint8_t st = 0;

  switch (st)
  {
    case 0:
      lcd.clear();
      lcd.setCursor(3, 0);               
      lcd.print("Press any key");
      lcd.setCursor(3, 2);
      lcd.print("for start game");
      st++;
      break;

    case 1:
      key = kpd.getKey();
      if (key != NO_KEY)
      { 
        _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
        st++;
      }
      break;

    case 2:                                           // ждем сообщения WiFi AP создана
      if (startWiFiAP())
      {
        st = 0;
        return true;
      }
      break;
  }
  return false;
}


void writeMACsToFlash() {
  // Запись числа зерегистрированных игроков каждой команды и МАС во Flash
  uint8_t team;
  uint8_t player_no;
  char player_name[4] = {' ', ' ', ' ', 0};

  // очищаем MAC всех игроков команд   
  for (team = 0; team < 2; team++)
  {
    if (team == RED)
      player_name[0]= 'R';
    else
      player_name[0] = 'B';
    for(player_no = 0; player_no < MAX_PLAYERS; player_no++)
    {
      player_name[1] = '0' + player_no / 10;
      player_name[2] = '0' + player_no % 10;
      preferences.putULong64(player_name, macs[team][player_no]);
    }
  }

  // Запись числа зерегистрированных игроков каждой команды во Flash.
  preferences.putUChar("RedPlayers",teams[RED].getGamePlayers());
  preferences.putUChar("BluePlayers",teams[BLUE].getGamePlayers());
}


uint8_t registerPlayers() {
  //----------------------------------------------------------------------------+
  //                   Регистрация игроков каждой команды.                      |
  // На дисплей выводится команда и порядковый номер игрока от 1 до MAX_PLAYERS.|
  // Игроки заходят в зону приема терминала и подключаются к нему.              |
  // Как только игрок на свем телефоне увидел успешное соединение - можно       |
  // отключаться. Так продолжается, пока все игроки комады пройдут регистрацию. |
  // Для перехода к регистрации игроков второй команды, необходимо, чтобы на    |
  // терминале не было подключенных игроков. Далее нажать кнопку "A". После     | 
  // этого можно регистрировать игроков второй команды. После окончания         |
  // регистрации и отключения всех игроков, нажать кнопку "*". Данные игроков   |
  // будут записаны во Flash память.                                            |
  // НЕ ДОПУСКАТЬ ОДНОВРЕМЕННОГО ПОДКЛЮЧЕНИЯ к терминалу БОЛЕЕ 10 ИГРОКОВ !     |
  //----------------------------------------------------------------------------+
  /*
   01234567890123456789
  ______________________
  |Wait...      BLUE 00|
  |                    |
  |Connected:        00|
  |A-next team   *-exit|
  |____________________|
  */

  static uint8_t st = 0;
  static uint8_t player;
  static uint8_t team;
  static uint16_t station_in_air;
  int8_t i8TeamPlayer;
  BaseType_t s;
  struct QueueItem qitem; // элемент очереди

  switch (st)
  {
    case 0:       // отрисовка статической инфо на странице
      lcd.clear();
      lcd.setCursor(0, 2);
      lcd.print(F("Connected:"));
      // lcd.setCursor(0, 2);    // строка МАС
      // lcd.print("MAC:");
      lcd.setCursor(0, 3);
      lcd.print(F("A-next team   *-exit"));

      clearMACsInRAM();
      teams[RED].clearTeamData();
      teams[BLUE].clearTeamData();
      teams[RED].setGamePlayers(0);
      teams[BLUE].setGamePlayers(0);

      station_in_air = 0;
      redrawTwoDigits(18,2,station_in_air);
      team = 0;
      st++;
      break;

    case 1:       // создать WiFi AP с уведомлением об успешном создании
      if (startWiFiAP()) st++;
      break;

    case 2:       // отрисовка названия команды
      drawTeamName(13,0,teams[team].sTeamName); // название команды
      player = teams[team].getGamePlayers();    // число игроков в команде
      lcd.setCursor(0, 0);
      if (player < MAX_PLAYERS)
        lcd.print(F("Wait...  "));
      else
        lcd.print(F("Go out..."));
      st++;
      break;

    case 3:       // отрисовка номера текущего игрока
      redrawTwoDigits(18,0,player + 1);                     // номер текущего игрока в строке 0
      // lcd.setCursor(8, 2);                      
      // lcd.print("            ");                            // стирание МАС адреса текущего игрока
      st++;
      break;

    case 4:       // ожидание подключения игрока
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      s = xQueueReceive(queue, &qitem, 0);
      // если данные из очереди получены
      if (s == pdPASS)
      {                
        // ищем МАС адрес - возвращает команду и номер игрока
        i8TeamPlayer = searchMACInArray(qitem.u64MAC);
        // если адрес найден
        if (i8TeamPlayer >= 0)
        {
          updatePlayerState(i8TeamPlayer, qitem.fConnect);
        }
        else    // если MAC адрес не найден
        {
          // если клиент подключился
          if (qitem.fConnect)// && player < MAX_PLAYERS)
          {
            // сохраняем МАС адрес клиента
            macs[team][player] = qitem.u64MAC;
            teams[team].updatePlayerState(player,qitem.fConnect);
            // отрисовать МАС-адрес подключенного клиента
            // String smac = uint64ToString(qitem.u64MAC);
            // drawMAC(8, 2, smac);
            // переход к след игроку команды
            if (++player < MAX_PLAYERS)
            {
              teams[team].setGamePlayers(player);
              st = 3;   // на нового игрока
            }
            else
            {
              teams[team].setGamePlayers(player);       // число игроков в команде
              lcd.setCursor(0, 0);
              lcd.print(F("Go out..."));
            }
          }
        }
        
        // if (qitem.fConnect)
        //   station_in_air++;
        // else if (station_in_air > 0)
        //   station_in_air--;


        station_in_air = teams[0].players_on_terminal + teams[1].players_on_terminal;                // количество игроков в зоне на текущем скане


        redrawTwoDigits(18,2,station_in_air);
        _tone32.playTone(2000, 200);
      }
      else if (!teams[RED].players_on_terminal && !teams[BLUE].players_on_terminal && !station_in_air)
      {
        key = kpd.getKey();
        if (key == '*' || key == 'A')
        { 
          _tone32.playTone(BUZZER_BUTTON, BUZZER_DURATION);
          //teams[team].setGamePlayers(player);       // число игроков в команде
          if (key == 'A') team ^= 0x01;             // на следующую  команду
          st = key == 'A' ? 2 : 5;                  // 2-на следующую  команду; 5-выйти из регистрации
        }
      }
      break;

    case 5:       // сохранить МАС-адреса во FLASH
      writeMACsToFlash();
      st++;
      break;

    case 6:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        return 1;
      }
      break;
  }

  return 0;  
}


// uint32_t calcDelayUpdateProgressBar(uint32_t nominal_delay_ms) {
//   int8_t num;

//   if (first_team == NONE) return nominal_delay_ms;

//   num = first_team == RED ? teams[RED].players_on_terminal - teams[BLUE].players_on_terminal : teams[BLUE].players_on_terminal - teams[RED].players_on_terminal;
    
//   if (num > 0) return nominal_delay_ms / num;
//   if (num < 0) return nominal_delay_ms * num;

//   // при равенстве игроков замедление на 50 %
//   return nominal_delay_ms + (nominal_delay_ms >> 1);  
// }


void UpdatePointState () {
  if (first_team == NONE)
  {
    if (teams[RED].players_on_terminal != teams[BLUE].players_on_terminal)
      first_team = teams[RED].players_on_terminal > teams[BLUE].players_on_terminal ? RED : BLUE;
  }
  else if (!teams[first_team].players_on_terminal)
    first_team = NONE;
}

void Demo() {
  /*----------------------------------------------------------------------------+
  // На дисплей выводится число игроков каждой команды, прошедших подключение.  |
  // Пройти подключение означает хотя-бы 1 раз подключиться к терминалу. Время  |
  // активации игрока равно 0 с. Повторное подключение игрока не учитывается.   |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |     DEMO MODE      |
  | RED = 00           |
  |BLUE = 00           |
  |    [*] - exit      |
  |____________________|
  */
  static uint8_t st = 0;          // Текущее состояние игры
  int8_t t;                       // команда
  struct QueueItem qitem;         // элемент очереди
  bool fChange;

  switch (st)
  {
    case 0:
      lcd.clear();
      lcd.setCursor(5, 0);               
      lcd.print("DEMO MODE");
      for (t = 0; t < 2; t++)
      {
        drawTeamName(0, t + 1,teams[t].sTeamName);
        lcd.print(" = ");
        redrawTwoDigits(7, t + 1, 0);
      }
      lcd.setCursor(4, 3);
      lcd.print("[*] - exit");
      st++;
      break;

    case 1:  // ожидание подключения игрока
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect);
      }

      fChange = false;
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected())
        {
          redrawTwoDigits(7, 1 + t, teams[t].marked_players_nums);
          if (!fChange) playTrack(TREK2, true);
          fChange = true;
        }
      }

      if (kpd.getKey() == '*') st++;
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        ESP.restart();
      }
      break;
  }         
}


bool Conquest(team_t * winteam) {         // GOFAST
  /*----------------------------------------------------------------------------+                    
  // В игре побеждает команда с большим числом игроков, прошедших подключение.  |
  // Пройти подключение означает хотя-бы 1 раз подключиться к терминалу. Время  |
  // подключения задается параметром “ACTIVATION TIME,S”. Игра продолжается,    |
  // пока не закончится время “GAME TIME”, или все игроки одной из команд       |
  // пройдут подключение. На дисплее отображается время до окончания игры и     |
  // число игроков каждой команды, прошедших подключение. Повторное подключение |
  // игрока запрещено. По окончании игры:                                       |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • на дисплей выводится TEAM RED WIN, TEAM ВLUE WIN или TIME OUT.           |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |      CONQUEST      |
  |      HH:MM:SS      |
  | RED = 00           |
  |BLUE = 00           |
  |____________________|
  */
  static uint8_t st = 0;           // Текущее состояние игры
  static uint32_t timeUpdate = 0;  // временной интервал для обновления индикаторов таймеров
  static uint8_t nums[2];          // Счетчики отсканированных карт
  static uint8_t total[2];         // общее кол-во игроков в команде
  int8_t t;                        // команда
  struct QueueItem qitem;          // элемент очереди
  uint32_t tm;
  bool fChange;
  
  switch (st)
  {
    case 0:
      lcd.clear();
      lcd.setCursor(6, 0);               
      lcd.print("CONQUEST"); 
      for (t = 0; t < 2; t++)
      {  
        drawTeamName(0, t + 2, teams[t].sTeamName);
        lcd.print(" = ");
        nums[t] = teams[t].marked_players_nums;
        total[t] = teams[t].getGamePlayers();
        redrawTwoDigits(7, t + 2, nums[t]);
      }
      // Таймер обратного отсчета
      secToTimeHHMMSS(game_timer.Secs(), 6, 1, true);
      game_timer.Start();
      st++;
      break;

    case 1:       // ожидание подключения/отключения игрока
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      fChange = false;
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
        {
          nums[t] = teams[t].marked_players_nums;
          if (nums[t] == total[t])
          {
            game_timer.Stop(); 
            st = 2;
          }
          redrawTwoDigits(7, t+2, nums[t]);
          if (!fChange) playTrack(TREK2, true);
          fChange = true;
        }
      }

      if (st != 2)
      {
        if (xTaskGetTickCount() > timeUpdate)
        {
          timeUpdate = xTaskGetTickCount() + 250;             // 250 мс - период обновления индикаторов таймеров  
          game_timer.Tick();                                  // Обновляем значение таймера
          secToTimeHHMMSS(game_timer.Secs(), 6, 1);           // Обновление индикатора времени до окончания игры
          if (game_timer.GetTime() == 0) st = 2;              // Если время игры истекло
        }
      }
      break;

    case 2:       // выдать уведомление Отключить WiFi АР, ждать ответное уведомление, затем определение исхода игры
      if (stopWiFiAP())
      {
        if (game_timer.GetTime() == 0)
        {
          if (nums[RED] != nums[BLUE])
            *winteam = nums[RED] > nums[BLUE] ? RED : BLUE;
        }
        else
        {
          if (nums[RED] == total[RED])
          {
            if (nums[BLUE] != total[BLUE])
              *winteam = RED;
            else if (total[RED] != total[BLUE])
              *winteam = total[RED] > total[BLUE] ? RED : BLUE;
          }
          else
            *winteam = BLUE;
        }
        st = 0;
        return true;
      }
      break;
  }
  return false;
}


bool Breakthrough(team_t * winteam) {     // SUPRESSION
  /*----------------------------------------------------------------------------+
  // На дисплее отображается остановленный таймер времени до окончания игры.    |
  // А такде счетчики игроков, не прошедших подключение игроков.                |
  // Пройти подключение означает хотябы 1 раз подключиться к терминалу. Время   |
  // подключения задается параметром “ACTIVATION TIME,S”. Команда, игрок которой|
  // подключился первым, запускает таймер. Эта команда становится нападающей.   |
  // Для победы все игроки нападающей команды должны пройти подключение до      |
  // завершения работы таймера. Вторая команда становится защищающей. Для победы|
  // игроки этой команды должны помешать нападающим выполнить свою миссию до    |
  // завершения работы таймера. В процессе игры на дисплей выводится время до   |
  // окончания игры и количество не подключенных игроков нападающей команды.    |
  // Игра продолжается, пока все игроки нападающией команды не пройдут          |
  // подключение или до завершения работы таймера. Повторное подключение игрока |
  // не учитывается. По окончании игры:                                         |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • на дисплей выводится TEAM RED WIN, TEAM ВLUE WIN или TIME OUT.           |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |    BREAKTHROUGH    |
  |      HH:MM:SS      |
  | RED = 00           |
  |BLUE = 00           |
  |____________________|
  */
  static uint8_t st = 0;                // Текущее состояние игры
  static uint32_t timeUpdate = 0;  // временной интервал для обновления индикаторов таймеров
  static uint8_t nums[2];               // Счетчики отсканированных карт
  static uint8_t total[2];              // общее кол-во игроков в команде
  int8_t t;                             // команда
  struct QueueItem qitem;   // элемент очереди
  uint32_t tm;
  
  switch (st)
  {
    case 0:
      lcd.clear();
      lcd.setCursor(4, 0);               
      lcd.print("BREAKTHROUGH");

      for (t = 0; t < 2; t++)
      {  
        drawTeamName(0, t + 2, teams[t].sTeamName);
        lcd.print(" = ");
        // кол-во игроков в каждой команде
        nums[t] = 0;
        total[t] = teams[t].getGamePlayers();
        redrawTwoDigits(7, t + 2, total[t]);
      }

      // Остановленный таймер обратного отсчета
      secToTimeHHMMSS(game_timer.Secs(), 6, 1, true);
      st++;
      break;

    case 1:                                           // ожидание прохождения всех игроков или окончания счета таймера
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
        {
          if (first_team == NONE)
          {
            first_team = t;
            playTrack(6 + t, true); // track 6=RED; 7=BLUE
            game_timer.Start();
          }

          if (first_team == t)
          { 
            if (teams[t].marked_players_nums != nums[t])
            {
              nums[t] = teams[t].marked_players_nums;
              if (nums[t] >= total[t])
                *winteam = (team_t) t;
              else
                redrawTwoDigits(7, t+2, total[t] - nums[t]);
              playTrack(TREK2, true);
            }
          }
        }
      }

      if (*winteam == NONE && first_team != NONE)
      {
        if (xTaskGetTickCount() > timeUpdate)
        {
          timeUpdate = xTaskGetTickCount() + 250;             // 250 мс - период обновления индикаторов таймеров  
          game_timer.Tick();                                // обновляем значение таймера
          secToTimeHHMMSS(game_timer.Secs(), 6, 1);         // обновление времени до окончания игры
          if (game_timer.GetTime() == 0)                    // если время игры истекло, то
            *winteam = first_team == RED ? BLUE : RED;          // определяем победителя
        }
      }  
      if (*winteam != NONE) st++;
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        return true; 
      }
      break;
  }
  return false;
}


bool Only_Me(team_t * winteam) {          // FUCING_FORCE
  /*----------------------------------------------------------------------------+
  // Программа рандомно назначает по одному счастливому игроку в каждой команде.|
  // Только эти игроки смогут остановить таймер обратного счёта во время игры.  |
  // На дисплее отображается остановленный таймер времени до окончания игры.    |
  // Первый подключенный игрок запускает таймер. Каждое новое подключение       |
  // (помимо счастливых игроков) ускоряет время в 2 раза. При подключении       |
  // счастливого игрока, время останавливается. Игра продолжается до подключения|
  // счастливая игрока любой из команд или до завершения работы таймера.        |
  // По окончании игры:                                                         |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • на дисплей выводится TEAM RED WIN, TEAM ВLUE WIN или TIME OUT.           |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |      ONLY ME       |
  |      HH:MM:SS      |
  |                    |
  |____________________|
  */
  static uint8_t st = 0;                  // Текущее состояние игры
  int8_t t;                             // команда
  struct QueueItem qitem;   // элемент очереди
  uint32_t tm;
  
  switch (st)
  {
    case 0:
      lcd.clear();
      lcd.setCursor(6, 0);               
      lcd.print("ONLY ME");     

      // Остановленный таймер обратного отсчета
      secToTimeHHMMSS(game_timer.Secs(), 6, 1, true);
      st++;
      break;

    case 1:                                           // ожидание первого сканирования и запуск таймера
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
        {
          // если игрок счастливый - заканчиваем игру
          if (bitRead(teams[t].GetMarkedPlayers(), teams[t].getLakePlayer())) // номер счастливого игрока в команде
          {
            *winteam = (team_t) t;
            st++;
            break;   
          }

          if (first_team == NONE)
          {
            first_team = t;
            game_timer.Start();
          }
          else
            game_timer.SetMult(game_timer.GetMult()*2.0); // скорость работы таймера увеличиваем в 2 раза        
          playTrack(TREK11, true); 
        }
      }  
 
      if (first_team != NONE)
      {
        game_timer.Tick();                                // обновляем значение таймера
        secToTimeHHMMSS(game_timer.Secs(), 6, 1);         // обновление времени до окончания игры   
        if (game_timer.GetTime() == 0) st++;              // если время игры истекло -> победителя нет
      }  
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        return true; 
      }
      break;
  }
  return false; 
}


bool Battle_Royale(team_t * winteam) {    // CONTRPOINT
  /*----------------------------------------------------------------------------+
  // Строка 1 - остановленный таймер обратного отсчёта.                         |
  // Строка 2 - счетчики подключений игроков каждой команды.                    |
  // Первое подключение игрока запускает таймер. Игроки могут подключаться      |
  // много раз. Интервал между подключениями одного и того же игрока задается   |
  // параметром “Pause scan,min:”. Если игрок подключается ранее этого времени, |
  // плейер запускает трек с сообщением об ошибке. Игра длится до завершения    |
  // работы таймера. Побеждает команда c большим числом подключений.            |
  // По окончании игры:                                                         |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • TEAM RED WIN!, TEAM ВLUE WIN! или TIME OUT! до выключения устройства.    |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |    BATTLE ROYALE   |
  |      HH:MM:SS      |
  | RED = 00           |
  |BLUE = 00           |
  |____________________|
  */
  static uint8_t st = 0;                // текущее состояние игры
  int8_t t;                             // команда
  struct QueueItem qitem;               // элемент очереди
  uint32_t tm;

  switch (st)
  {
    case 0:                             // отрисовка экрана игры, инициализация переменных
      lcd.clear();
      lcd.setCursor(4, 0);               
      lcd.print("BATTLE ROYALE");

      // счетчики отсканированных карт
      for (t = 0; t < 2; t++)
      {  
        drawTeamName(0, t + 2, teams[t].sTeamName);
        lcd.print(" = ");
        redrawTwoDigits(7, t + 2, teams[t].point_scan_count);
      }

      // остановленный таймер обратного отсчета
      secToTimeHHMMSS(game_timer.Secs(), 6, 1, true);
      st++;
      break;

    case 1:                               // ожидание первого сканирования и запуск таймера
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
        {
          if (first_team == NONE)
          {
            first_team = t;
            game_timer.Start();
          }
          playTrack(6 + t, true); // track 6=RED team; 7=BLUE team
          redrawTwoDigits(7, t+2, teams[t].point_scan_count);
        }
      }

      if (first_team != NONE)
      {
        game_timer.Tick();                                // обновляем значение таймера
        secToTimeHHMMSS(game_timer.Secs(), 6, 1);         // обновление времени до окончания игры
        if (game_timer.GetTime() == 0)                    // если время игры истекло, то
        {
          // определяем победителя
          if (teams[RED].point_scan_count != teams[BLUE].point_scan_count)
            *winteam = teams[RED].point_scan_count > teams[BLUE].point_scan_count ? RED : BLUE;
          st++;
        }
      }
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        return true; 
      }
      break;
  }
  return false; 
}


bool Sabotage(team_t * winteam) {         // ADDTIME
  /*----------------------------------------------------------------------------+
  // Строки 1 & 2 - таймеры обратного отсчёта для каждой команды.               |
  // При старте в таймеры заносится время игры и они начинают обратный отсчет.  |
  // При подключении игрока в таймер противника добавляется штрафное время      |
  // penalty_time_ms, при условии что предыдущее подключение было выполнено     |
  // этой же командой. Повторное подключение игрока не допускается.             |
  // Побеждает команда, чей таймер обнулится первым.                            |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |      SABOTAGE      |
  | RED = HH:MM:SS     |
  |BLUE = HH:MM:SS     |
  |                    |
  |____________________|
  */
  static uint8_t st = 0;                // текущее состояние игры
  struct QueueItem qitem;               // элемент очереди
  int8_t t;                             // команда
  uint32_t tm;
  static uint32_t back_sec[2];          // время на таймерах обратного счета

  switch (st)
  {
    case 0:                             // отрисовка экрана игры, инициализация переменных
      lcd.clear();
      lcd.setCursor(6, 0);               
      lcd.print("SABOTAGE");

      // остановленные таймеры обратного отсчета
      for (t = 0; t < 2; t++)
      {
        drawTeamName(0, t + 1, teams[t].sTeamName);
        lcd.print(" = ");
        team_timer[t].SetTime(game_time_ms);             // таймер обратного счета красных
        back_sec[t] = team_timer[t].Secs();
        secToTimeHHMMSS(back_sec[t], 7, t + 1, true);
      }
      st++;
      break;

    case 1:        // индикация таймеров обратного счета красных и синих
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
        {
          playTrack(6 + t, true); // track 6=RED team; 7=BLUE team
          if (first_team == t)
          {  
            team_timer[t ^ 0x01].SetTime(team_timer[t ^ 0x01].GetTime() + penalty_time_ms);
            back_sec[t ^ 0x01] = team_timer[t ^ 0x01].Secs();
            secToTimeHHMMSS(back_sec[t ^ 0x01], 7, (t ^ 0x01) + 1, true);
          }
          else
          {
            first_team = t;
            team_timer[t].Start();
            team_timer[t ^ 0x01].Stop();
          }
        }
      }     

      if (first_team != NONE)
      {
        team_timer[first_team].Tick();
        if (back_sec[first_team] != team_timer[first_team].Secs())
        {
          back_sec[first_team] = team_timer[first_team].Secs();
          secToTimeHHMMSS(back_sec[first_team], 7, first_team + 1, true);
        }
      }
 
      if (!team_timer[RED].GetTime() || !team_timer[BLUE].GetTime())  // Если время игры истекло
      { 
        back_sec[RED] = team_timer[RED].GetTime();       
        back_sec[BLUE] = team_timer[BLUE].GetTime();
        if (back_sec[RED] != back_sec[BLUE])
          *winteam = back_sec[RED] < back_sec[BLUE] ? RED : BLUE;
        st++;
      }
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        st = 0;
        return true; 
      }
      break;
  }
  return false; 
}


bool Domination(team_t * winteam) {       // DOMIN
  /*----------------------------------------------------------------------------+
  // В верхней строке отображается таймер обратного отсчёта времени игры. В     |
  // нижней строке таймеры времени доминирования для каждой команды.            |
  // Побеждает команда, чей таймер показывает большее время доминирования.      |
  // То есть все захваты, которые проводила команда суммируются, и таким образом|
  // понятен победитель.                                                        |
  // Активация запускается той командой, чей игрок первым попадает в зону приема|
  // точки. Например, появился игрок R02 – красные запустили активацию          |
  // Скорость работы таймера активации зависит от количества игроков в зоне     |
  // приема. Активация прерывается, если нет ни одного игрока из команды,       |
  // запустившей активацию. Активация завершается, если отработал таймер        |
  // активации. Таймер доминирования захватившей точку команды (RED)            |
  // начинает наращивать свои показания. Повторное подключение игрока разрешено.|
  // По окончании игры:                                                         |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • TEAM RED WIN!, TEAM ВLUE WIN! или TIME OUT! до выключения устройства.    |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |DOMINATION  HH:MM:SS|
  | RED = HH:MM:SS     |
  |BLUE = HH:MM:SS     |
  |                    |
  |____________________|
  */
  static uint8_t st = 0;                // текущее состояние игры
  struct QueueItem qitem;               // элемент очереди
  int8_t t;                             // команда
  static int32_t nominal_delay_ms;
  static int16_t pos;                   // 5*20 (5 пикселей ширина символа * 20 символов в строке) 
  static uint32_t pb_timer = 0;         // таймер для отрисовки ProgressBar
  static uint32_t toneInterval;         // временной интервал для бузера
  static uint32_t timeUpdate = 0;         // временной интервал для обновления индикаторов таймеров

  switch (st)
  {
    case 0:
 //log_i("portTICK_PERIOD_MS = %d",  portTICK_PERIOD_MS);
      lcd.clear();             
      lcd.print("DOMINATION");
      pos = 100;                                            // = 5*20 (5 пикселей ширина символа * 20 символов в строке)
      nominal_delay_ms = activation_time_ms / pos;          // за один такт будет отрисован прямоугольник шириной 5 пикселей
      toneInterval = pos * activation_time_ms / 1000UL;

      for (t = 0; t < 2; t++)
      {
        drawTeamName(0, t + 1, teams[t].sTeamName);
        lcd.print(" = ");
        // установка прямого счета таймеров команд
        team_timer[t].SetDir(true);
        team_timer[t].SetTime(0);
        secToTimeHHMMSS(team_timer[t].Secs(), 7, t + 1, true);
      }

      // Таймер обратного отсчета
      secToTimeHHMMSS(game_timer.Secs(), 12, 0, true);
      game_timer.Start();
      st++;
      break;

    case 1:                             // очистка ProgressBar
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      pos = 100;                    // = 5*20 (5 пикселей ширина символа * 20 символов в строке)
      //pb_timer = xTaskGetTickCount() + calcDelayUpdateProgressBar(nominal_delay_ms);
      st++;
      break;

    case 2:                           // ожидание подключения первого игрока
      if (first_team != NONE)
      {
       // pb_timer = xTaskGetTickCount() + calcDelayUpdateProgressBar(nominal_delay_ms);
        pb_timer = xTaskGetTickCount();         // таймер для отрисовки ProgressBar
        playTrack(20 + first_team, true); // 20=RED activated; 21=BLUE activated
        st++;    // -> на захват точки
      }
      break;

    case 3: // процесс захвата точки - должен быть подключен не менее времени активации // в это время отображается прогрессбар
      if (first_team != NONE && first_team != whose_point)
      {     
        if (xTaskGetTickCount() >= pb_timer)               // отрисовка progress bar
        {
          // pb_timer = xTaskGetTickCount() + calcDelayUpdateProgressBar(nominal_delay_ms);
          //timer = timer + nominal_delay_ms;
          pb_timer += nominal_delay_ms;

          int8_t c = 20 - pos / 5;
          if (c < 20)
          {
            lcd.setCursor(c, 3);
            lcd.print((char)(4 - pos % 5));
          }

          if (pos % toneInterval == 0)
            _tone32.playTone(BUZZER_FREQUENCY, 100);       
        
          if(!--pos)  // точка захвачена
          { 
            whose_point = first_team;
            team_timer[whose_point].Start();
            playTrack(22 + whose_point, true);      // 22-Red timer run; 23-Blue timer run
            // очистка ProgressBar
            lcd.setCursor(0, 3);
            lcd.print("                    ");
            //pos = 100;                    // = 5*20 (5 пикселей ширина символа * 20 символов в строке)
            st++;
          }
        }
      }
      else
      {
        log_i("st = %d", st);
        st = 1;
      }
      break;

    case 4:  // точка захвачена - удержание
      if (whose_point != first_team)
      {
        st = 1;
      } 
      break;

    case 5:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        if (team_timer[RED].Secs() != team_timer[BLUE].Secs())
          *winteam = team_timer[RED].Secs() > team_timer[BLUE].Secs() ? RED : BLUE;
        st = 0;
        return true; 
      }
      break;
  }

  if (st > 0 && st < 5) // уточнить номер состояния 6 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  {
    // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
    if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
    {
      int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
      if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
      {
        updatePlayerState(i8TeamPlayer, qitem.fConnect);
        if (qitem.fConnect) _tone32.playTone(2000, 150);
        // UpdatePointState(); 
      }
    }

    UpdatePointState();

    if (xTaskGetTickCount() > timeUpdate)
    {
      timeUpdate = xTaskGetTickCount() + 250; 
      if (whose_point != NONE)
      {
        team_timer[whose_point].Tick();
        secToTimeHHMMSS(team_timer[whose_point].Secs(), 7, whose_point + 1);
      } 

      game_timer.Tick();                                  // Обновляем значение таймера
      secToTimeHHMMSS(game_timer.Secs(), 12, 0);
      if (game_timer.GetTime() == 0)                      // Если время игры истекло
      {
        team_timer[RED].Stop();
        team_timer[BLUE].Stop();
        st = 5;     // уточнить номер состояния определения победителя 6
      }
    }
  }
  
  return false; 
}


bool HardPoint(team_t * winteam) {       // HARDPOINT
  /*----------------------------------------------------------------------------+
  // В верхней строке отображается таймер обратного отсчёта времени игры.       |
  // В нижней строке счетчики очков для каждой команды.                         |
  // Этот режим похож на режим Domination, но вместо учета времени доминирования|
  // в нем учитываются очки, набранные командой. Если игрок попал в зону приема |
  // точки, и  находится там не менее 3 с, он приносит своей команде N кол-во   |
  // очков (задается параметром “Player points” перед началом игры). Программа  |
  // рандомно назначает по одному счастливому игроку в каждой команде.          |
  // Cчастливый игрок приносит своей команде удвоенное количество очков.        |
  // Параметр “Reenter time, s” задает время, которое игрок должен провести вне |
  // зоны приема точки, чтобы при повторном заходе он снова принес очки своей   |
  // команде. Если установить значение этого параметра в 0, то очки будут       |
  // учитываться только при первом заходе в зону. Побеждает команда, которая по |
  // истечении времени игры, наберет большее количество очков.                  |
  // По окончании игры:                                                         |
  // • на 1 минуту включается реле, управляющее сиреной;                        |
  // • TEAM RED WIN!, TEAM ВLUE WIN! или TIME OUT! до выключения устройства.    |
  //----------------------------------------------------------------------------+
   01234567890123456789
  ______________________
  |      HARDPOINT     |
  |      HH:MM:SS      |
  | RED = 00000        |
  |BLUE = 00000        |
  |____________________|
  */
  static uint8_t st = 0;                // текущее состояние игры
  static uint32_t timeUpdate = 0;       // временной интервал для обновления индикаторов таймеров
  struct QueueItem qitem;               // элемент очереди
  int8_t t;                             // команда
  uint32_t tm;                          // таймер

  switch (st)
  {
    case 0:                             // отрисовка экрана игры, инициализация переменных
      lcd.clear();
      lcd.setCursor(6, 0);
      lcd.print("HARDPOINT");
      for (t = 0; t < 2; t++)
      {
        drawTeamName(0, t + 2, teams[t].sTeamName);
        lcd.print(" = ");
        redrawFiveDigits(7, t + 2, teams[t].point_scan_count);  // кол-во очков каждой команды
      }
      activation_time_ms = 3000;                          // для этого режима всегда 3 сек
      secToTimeHHMMSS(game_timer.Secs(), 6, 1, true);    // Таймер времени игры
      game_timer.Start();
      st++;
      break;

    case 1:
      // Читаем данные из очереди. Значение 0 в 3-м параметре означает, что не ждем, если очередь пуста.
      if (xQueueReceive(queue, &qitem, 0) == pdPASS)          // если данные из очереди получены
      {
        int8_t i8TeamPlayer = searchMACInArray(qitem.u64MAC); // ищем МАС адрес среди игрока
        if (i8TeamPlayer >= 0)                                // если МАС принадлежит одному из игроков
          updatePlayerState(i8TeamPlayer, qitem.fConnect); 
      }

      tm = xTaskGetTickCount();
      for (t = 0; t < 2; t++)
      {
        if (teams[t].checkTimeConnected(tm))
          redrawFiveDigits(7, t + 2, teams[t].point_scan_count);    // кол-во очков каждой команды
      } 

      if (xTaskGetTickCount() > timeUpdate)
      {
        timeUpdate = xTaskGetTickCount() + 250;                     // 250 мс - период обновления индикаторов таймеров
        game_timer.Tick();                                          // Обновляем таймер времени игры
        secToTimeHHMMSS(game_timer.Secs(), 6, 1);
        if (game_timer.GetTime() == 0) st++;                        // Если время игры истекло
      }
      break;

    case 2:       // выдать уведомление Отключить WiFi АР и ожидать ответное уведомление
      if (stopWiFiAP())
      {
        if (teams[RED].point_scan_count != teams[BLUE].point_scan_count)
          *winteam = teams[RED].point_scan_count > teams[BLUE].point_scan_count ? RED : BLUE;
        st = 0;
        return true; 
      }
      break;
  }
  return false;
}


void drawResult(team_t winteam, int8_t * mode) {
  // отрисовка исхода игры + проигрывание трека
  String s = "";
  int8_t trek = 0;

  if (winteam == NONE)
  {
    switch (*mode)
    {
      case HARDPOINT:
        s = "  DRAW! FANTASTIC!"; break;
      default:
        s = "     TIME OUT!"; break;
    }
    trek = TREK8; // Game over
  }
  else
  {
    s = winteam == RED ? "    RED TEAM WIN" : "    BLUE TEAM WIN";
    trek = 4 + winteam;// track 4=RED teaw win; 5=BLUE teaw win 
  }

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(s);
  playTrack(trek, true);
  while (!playTrack(0)) {;} // ждем окончания проигрывания трека
  digitalWrite(RELAY_PIN, ON);               // включаем реле
  vTaskDelay(pdMS_TO_TICKS(RELAY_ON_MS));
  digitalWrite(RELAY_PIN, OFF);
  lcd.setCursor(5, 3);
  lcd.print(F("[*] - exit"));
}

#endif  // _FUNCTIONS_H_
