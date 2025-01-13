#include "Arduino.h"
#include <FS.h>
#include <SPIFFS.h>
#include "stm32_ota.h"
#include "./WEB/Web.h"

DEV_Wifi wifi;

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
    stm32_ota &ota = stm32_ota::getInstance(16, 15, 9, 10, 11, &Serial1, 115200);
    ota.begin();      // Khởi tạo UART và cấu hình GPIO
    wifi.initWiFi();
    wifi.connectWiFi();
}
void loop()
{
    // Không làm gì trong vòng lặp
}
