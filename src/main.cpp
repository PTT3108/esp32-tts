#include "Arduino.h"

#if defined(PLATFORM_ESP8266)
#include <FS.h>
#elif defined(PLATFORMIO_ESP32)
#include <SPIFFS.h>
#endif
#include "./WEB/Web.h"
#include "devButton.h"
#include "SENSOR/devSensor.h"
#include "STM32_UPDATE/dev_Stm32_Update.h"
#include "helps.h"

#include "PWM.h"

device_affinity_t devices[] = {
    {&Button_device, 0},
    {&Stm32Update_device, 1},
    {&Sensor_dev,0}};

DEV_Wifi wifi;
Stream *Backpack;
Stream *NodeUSB;

#if defined(PLATFORM_ESP32_S3)
#include "USB.h"
#define USBSerial Serial
#endif


void setupSerial()
{
    Stream *serialPort;
#if defined(PLATFORM_ESP32)
    serialPort = new HardwareSerial(0);
    ((HardwareSerial *)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1);
#elif defined(PLATFORM_ESP8266)
    if (GPIO_PIN_DEBUG_TX != UNDEF_PIN)
    {
        serialPort = new HardwareSerial(1);
        ((HardwareSerial *)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, SERIAL_TX_ONLY, GPIO_PIN_DEBUG_TX);
    }
    else
    {
        serialPort = new NullStream();
    }
#endif
    Backpack = serialPort;
#if defined(PLATFORM_ESP32_S3)
    Serial.begin(115200);
#endif

// Setup NodeUSB
#if defined(PLATFORM_ESP32_S3)
    USBSerial.begin(115200);
    NodeUSB = &USBSerial;
#elif defined(PLATFORM_ESP32)
    if (GPIO_PIN_DEBUG_RX == 3 && GPIO_PIN_DEBUG_TX == 1)
    {
        // The backpack is already assigned on UART0 (pins 3, 1)
        // This is also USB on modules that use DIPs
        // Set NodeUSB to TxBackpack so that data goes to the same place
        NodeUSB = NodeBackpack;
    }
    else if (GPIO_PIN_RCSIGNAL_RX == U0RXD_GPIO_NUM && GPIO_PIN_RCSIGNAL_TX == U0TXD_GPIO_NUM)
    {
        // This is an internal module, or an external module configured with a relay.  Do not setup NodeUSB.
        NodeUSB = new NullStream();
    }
    else
    {
        // The backpack is on a separate UART to UART0
        // Set NodeUSB to pins 3, 1 so that we can access NodeUSB and TxBackpack independantly
        NodeUSB = new HardwareSerial(1);
        ((HardwareSerial *)NodeUSB)->begin(firmwareOptions.uart_baud, SERIAL_8N1, 3, 1);
    }
#else
    NodeUSB = new NullStream();
#endif
}
void setup()
{
    pwm.init_pwm();
    setupSerial();

    SPIFFS.begin(true);

    
    wifi.initWiFi();
    wifi.connectWiFi();
    devicesRegister(devices, ARRAY_SIZE(devices));
    devicesInit();
    devicesStart();

    registerButtonFunction(ACTION_UPLOAD_ESP32, []()
                           { ESP_LOGI("Button_Action", "ESP32 Update"); });
}
void loop()
{
    unsigned long now = millis();
    devicesUpdate(now);
    // Không làm gì trong vòng lặp
}
