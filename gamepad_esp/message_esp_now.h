#ifndef _MESSAGE_ESP_NOW_H_
#define _MESSAGE_ESP_NOW_H_

/*-------------------------------------------------------
Отправка сообщения.
espnow_event_t send_evt;        // событие на отправку

// событие
typedef struct {
    uint8_t id_cb;          // callback-функция, поместившая сообщение в очередь
    uint8_t status;         // состояние
    espnow_msg_t msg;
} espnow_event_t;

// сообщение в очереди
typedef struct {
    uint8_t cmd;
    uint8_t data[2];
} espnow_msg_t;

1. Подготовка сообщения и события
Главная задача:
1.1 ожидает обнуления таймера интервала между сообщениями (500 ms)
    if (xTaskGetTickCount() - tm > 500)
1.2 записывает команду
    send_evt.msg.cmd = PING;
1.3 записывает номер peer
    send_evt.msg.data[0] = 0;           // (0=red; 1=blue)
1.4 очищает статус и устанавливает флаг готовнсти к передаче
    send_evt.status = MSG_RDY_TO_SEND;
1.5 запускает таймер интервала между передачей сообщения
    tm = xTaskGetTickCount();

2. Передача события
Главная задача:
2.1 проверяет статус события на готовнсть к передаче и отправляет
    уведомление задаче WiFi на передачу сообщения
        if (evt->status == MSG_RDY_TO_SEND) {
            xTaskNotify(hTaskWiFi, NTF_SEND_WIFI, eSetBits);

3. Ожидание уведомления о завершении обмена
Главная задача:
3.1 ожидание уведомления NTF_SEND_FINAL от задачи WiFi
    if (xTaskNotifyWait(0, NTF_SEND_FINAL, &rv, 0) == pdTRUE) 
3.2 установка флага в статусе события 
        evt->status |= MSG_SEND_FINAL;




WiFi задача:








направление передачи:   gamepad => base
u8Cmd = Ping
u8Data[0] = номер базы 0-RED; 1-BLUE
u8Data[1] = 

Ответ    gamepad <= base
u8Cmd = Ping
u8Data[0] = номер базы 0-RED; 1-BLUE
u8Data[1] = состояние плейера 0-не инициализирован; 1-играет трек; 2-готов

---------------------------------------------------------
направление передачи:   gamepad => base
u8Cmd = PlayTrack (проиграть трек)
u8Data[0] = номер базы
u8Data[1] = номер трека

Ответ    gamepad <= base
u8Cmd = PlayTrack
u8Data[0] = номер базы
u8Data[1] = состояние плейера 0-не инициализирован; 1-играет трек; 2-готов
-------------------------------------------------------*/

#define ESPNOW_MAXDELAY       512
#define ESPNOW_QUEUE_SIZE     6

/*---------------------------------------------------------
Статус события
*/
const uint8_t   MSG_EMPTY       = 0b00000000;               // пустое
const uint8_t   MSG_RDY_TO_SEND = 0b00000001;               // сообщение сформировано и готово к отправке
const uint8_t   MSG_PUT_SEND_CB = 0b00000010;               // сообщение передано на отправку в callback-функцию
const uint8_t   MSG_SEND_OK     = 0b00000100;               // сообщение отправлено успешно
const uint8_t   MSG_RECV_OK     = 0b00001000;               // ответное сообщение принято успешно
const uint8_t   MSG_SEND_FINAL  = 0b10000000;               // передача сообщения завершена

typedef enum {
    ESPNOW_SEND_CB,
    ESPNOW_RECV_CB,
} espnow_event_id_t;

const uint8_t PING          = 1;
const uint8_t PLAY_TRACK    = 2;

typedef struct {
    uint8_t cmd;            // команда
    uint8_t data[2];        // data[0] - номер peer: (0=red; 1=blue; 255=broadcast)
                            // data[1] - номер трека: (1-255)
} espnow_msg_t;


typedef struct {
    uint8_t id_cb;          // callback-функция, поместившая сообщение в очередь
    uint8_t status;         // состояние
    espnow_msg_t msg;       // сообщение
} espnow_event_t;


espnow_event_t send_evt;
espnow_event_t recv_evt;

#define		NB_ITEMS	5               // количество отправляемых сообщений в очереди
// espnow_msg_t q_out_msg [NB_ITEMS];   

	// **	\param [in] size_rec - size of a record in the queue
	// **	\param [in] nb_recs - number of records in the queue
	// **	\param [in] type - cppQueue implementation type: FIFO, LIFO
	// **	\param [in] overwrite - Overwrite previous records when queue is full
	// **	\param [in] pQDat - Pointer to static data queue
	// **	\param [in] lenQDat - Length of static data queue (in bytes) for static array size check against required size for queue
	// **	\return nothing
	
cppQueue q_out_msg(sizeof(espnow_msg_t), NB_ITEMS, IMPLEMENTATION);             // очередь отправляемых сообщений

static TaskHandle_t hTaskWiFi;
static TaskHandle_t hTaskMain;

const uint8_t NTF_START_WIFI    = 0b00000001;
const uint8_t NTF_ERROR_WIFI    = 0b00000010;
const uint8_t NTF_STOP_WIFI     = 0b00000100;
const uint8_t NTF_SEND_WIFI     = 0b00001000;
const uint8_t NTF_SEND_FINAL    = 0b00010000;           // передача сообщения завершена (успешная или нет)
// const uint8_t NTF_SEND_FAIL     = 0b01000000;
// const uint8_t NTF_RECV_ACK_OK   = 0b10000000;       // ответное сообщение принято успещно

#endif