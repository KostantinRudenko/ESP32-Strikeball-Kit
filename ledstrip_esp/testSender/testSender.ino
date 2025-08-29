
#pragma region ______________________________ Includes

#include <WiFi.h>
#include <esp_now.h>
#include "macs.h"

#pragma endregion Includes

#pragma region ______________________________ Constants

#define GamepadMAC "80:F3:DA:61:AC:70"

typedef struct {
    uint8_t cmd;
    uint8_t data[2];
} msg_esp_now_t;

enum Commands {
    Ping = 1,
    CMD_FillStrip = 3,
    CMD_FillStripByProgress,
    CMD_ClearStrip
};

#pragma engregion Constants

#pragma region ______________________________ Variables

bool res;

uint8_t G_aru8GamepadMAC[6];

esp_now_peer_info_t peerInfo;

#pragma endregion Variables

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("sent successfully");
    } else {
        Serial.println("sent NOT successfully");
    }
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

    return true;
}

void setup() {

    Serial.begin(9600);

    MacStringToByteArray(GamepadMAC, G_aru8GamepadMAC);

    connectToWiFi();

    Serial.println("init end");
}

void loop() {
    msg_esp_now_t msg;

    msg.cmd = CMD_FillStripByProgress;
    msg.data[0] = 0;
    msg.data[1] = 40;

    esp_now_send(G_aru8GamepadMAC, (uint8_t *) &msg, sizeof(msg_esp_now_t));
    
    delay(2000);

    msg.cmd = CMD_ClearStrip;

    esp_now_send(G_aru8GamepadMAC, (uint8_t *) &msg, sizeof(msg_esp_now_t));

    delay (2000);
}
