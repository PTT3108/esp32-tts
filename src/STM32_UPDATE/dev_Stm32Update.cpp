#include "dev_Stm32_Update.h"
#include "stm32_ota.h"
#include "config.h"
#include "common.h"
#include "devButton.h"
stm32_ota &ota = stm32_ota::getInstance(16, 15, 9, 10, 11, &Serial1, 115200);
void STM32_Update()
{
    ota.FileUpdate(STM32_FRIMWARE_PATH);
}
static bool initialize()
{
    ota.begin();   // Khởi tạo UART và cấu hình GPIO
    ota.RunMode(); // Chạy chế độ OTA
    registerButtonFunction(ACTION_UPLOAD_STM32, []()
                           { STM32_Update(); });
    return true;
}
static int start(){
    return DURATION_NEVER;
}
static int event(){
    ESP_LOGI("STM32 Update","STM32_EVENT");
    Stm32_Target_Found = ota.conect();
        ESP_LOGI("STM32_update", "Uploads info: %s", Stm32_Target_Found);
    if (Stm32_Target_Found != "ERROR")
    {
        boolean sussces = ota.EraseChip() && ota.Flash(STM32_FRIMWARE_PATH);
        ota.RunMode();
        ESP_LOGI("STM32_update", "Update complete, rebooting %d", sussces);
    }    
    return DURATION_NEVER;
}

device_t Stm32Update_device{
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = nullptr,
    .subscribe = EVENT_UPDATE_FIRMWARE_STM32};