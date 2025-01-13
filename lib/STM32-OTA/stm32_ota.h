#pragma once
#include "stdio.h"
#include "helps.h"

class stm32_ota
{
public:

    // Hàm getInstance để lấy thể hiện duy nhất của lớp
    static stm32_ota &getInstance(uint8_t rx = GPIO_NUM_NC, uint8_t tx = GPIO_NUM_NC, uint8_t rst = GPIO_NUM_NC, uint8_t boot0 = GPIO_NUM_NC, uint8_t led = GPIO_NUM_NC, Stream *uart = nullptr, uint32_t baud = UART_BAUD_RATE)
    {
        static stm32_ota instance(rx, tx, rst, boot0, led, uart, baud); // Thể hiện duy nhất của lớp
        return instance;
    }

    void setUART(Stream *uart, uint32_t baud)
    {
        this->OTA_uart = uart;
        this->OTA_Baud = baud;
        ESP_LOGI("STM32_OTA", "UART initialized with baud rate: %d", OTA_Baud);
    }
    void begin();
    String otaUpdate(String File_Url);
    void FlashMode();
    void RunMode();
    String conect();
    char chipVersion();
    String binfilename();
    boolean downloadFile(String File_Url, String File_Name);
    void deletfiles(String bin_file);
    boolean Flash(String bin_file_name);
    boolean EraseChip();
    String stm32Read(unsigned long rdaddress, unsigned char rdlen);
    void FileUpdate(String file_path);
    int hasError()
    {return Sussces;}

private:

    uint8_t OTA_RX = GPIO_NUM_NC;    // GPIO pin for RX
    uint8_t OTA_TX = GPIO_NUM_NC;    // GPIO pin for TX
    uint8_t OTA_RST = GPIO_NUM_NC;   // GPIO pin for Reset
    uint8_t OTA_BOOT0 = GPIO_NUM_NC; // GPIO pin for BOOT0
    uint8_t OTA_LED = GPIO_NUM_NC;
    uint32_t OTA_Baud = UART_BAUD_RATE; // Default UART baud rate
    Stream *OTA_uart = nullptr;         // Pointer to UART stream

    int Sussces = -1;
    const String STM32_CHIPNAME[8] = {
        "Unknown Chip",
        "STM32F03xx4/6",
        "STM32F030x8/05x",
        "STM32F030xC",
        "STM32F103x4/6",
        "STM32F103x8/B",
        "STM32F103xC/D/E",
        "STM32F105/107"};
    void SendCommand(unsigned char commd);
    unsigned char GetId();
    unsigned char Erase();
    unsigned char Erasen();
    unsigned char Address(unsigned long addr);
    unsigned char SendData(unsigned char *data, unsigned char wrlen);
    unsigned char getChecksum(unsigned char *data, unsigned char datalen);
        // Constructor để khởi tạo GPIO
    stm32_ota(uint8_t rx, uint8_t tx, uint8_t rst, uint8_t boot0 = GPIO_NUM_NC, uint8_t led = GPIO_NUM_NC, Stream *uart = nullptr, uint32_t baud = UART_BAUD_RATE);
};
