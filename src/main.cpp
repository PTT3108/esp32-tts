#include "Arduino.h"
#include <FS.h>
#include <SPIFFS.h>
#include "stm32_ota.h"
#include "./WEB/Web.h"
#include "devButton.h"

stm32_ota &ota = stm32_ota::getInstance(16, 15, 9, 10, 11, &Serial1, 115200);
device_affinity_t devices[] = {
    {&Button_device, 0},
};

DEV_Wifi wifi;
void STM32_Update(){
    ESP_LOGI("Button_Action","STM32 Update");
    ota.FileUpdate(STM32_FRIMWARE_PATH);
}

void setup()
{
    Serial.begin(115200);
    // Khởi tạo SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to mount file system");
        return;
    }
    // Lấy thể hiện duy nhất của stm32_ota
    ota.begin(); // Khởi tạo UART và cấu hình GPIO
    wifi.initWiFi();
    wifi.connectWiFi();
    devicesRegister(devices, ARRAY_SIZE(devices));
    devicesInit();
    devicesStart();
    registerButtonFunction(ACTION_UPLOAD_STM32, []()
                           { STM32_Update(); });
     registerButtonFunction(ACTION_UPLOAD_ESP32, []()
                           { ESP_LOGI("Button_Action","ESP32 Update"); });
}
void loop()
{
    unsigned long now = millis();
    devicesUpdate(now);
    // Không làm gì trong vòng lặp
}
