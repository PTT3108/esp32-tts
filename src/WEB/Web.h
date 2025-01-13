#pragma once
#include "string.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

class DEV_Wifi
{
public:
    static DEV_Wifi &getInstance()
    {
        static DEV_Wifi instance;
        return instance;
    }
    bool initWiFi();
    void connectWiFi();
    // bool disconnectWiFi();
    // bool isConnected();


private:
    const char *ssid = DEFAULT_WIFI_SSID;
    const char *password = DEFAULT_WIFI_PASSWORD;
    const char *path = "/download/blink.bin"; // Đường dẫn tệp
    // Lưu trữ tệp tải về
    const char *filename = "/stm32ota/ota.bin";

    // void wifi_init_sta();
    // bool restartWiFiData();


    TaskHandle_t wifi_TaskHandle = NULL;

    // void createWiFiTask();
    // static void wifi_task(void *);
    bool connected = false;
    bool initialized = false;
    void startServices();
    void HandleWeb();
    // bool addWiFiCredentialsFromConfig();
};