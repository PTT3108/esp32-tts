#include "stm32_ota.h"
#include "stm32_ota_commad.h"

#include "esp_log.h"
#include "SPIFFS.h"
#define STM32_OTA_TAG "STM32_OTA"
char rx_buffer[64];
int bytes_buffer;
String bin_file_name;

stm32_ota::stm32_ota(uint8_t rx, uint8_t tx, uint8_t rst, uint8_t boot0, uint8_t led, Stream *uart, uint32_t baud)
    : OTA_RX(rx), OTA_TX(tx), OTA_RST(rst), OTA_BOOT0(boot0), OTA_LED(led), OTA_uart(uart), OTA_Baud(baud) // Gán giá trị thông qua danh sách khởi tạo
{
    ESP_LOGI("STM32_OTA", "GPIO initialized: RX=%d, TX=%d, RST=%d, BOOT0=%d", OTA_RX, OTA_TX, OTA_RST, OTA_BOOT0);
}

void stm32_ota::begin()
{
    ESP_LOGI(STM32_OTA_TAG, "Starting STM32 OTA process...");

    // Thiết lập UART mặc định (UART1)
    if (OTA_uart == nullptr && OTA_RX != GPIO_NUM_NC && OTA_TX != GPIO_NUM_NC)
    {
        ESP_LOGI(STM32_OTA_TAG, "Using default UART1.");
        OTA_uart = new HardwareSerial(1);
        ((HardwareSerial *)OTA_uart)->begin(OTA_Baud, SERIAL_8E1, OTA_RX, OTA_TX);
    }
    else
    {
        ESP_LOGI(STM32_OTA_TAG, "Using custon uart.");
        ((HardwareSerial *)OTA_uart)->begin(OTA_Baud, SERIAL_8E1, OTA_RX, OTA_TX);
    }

    pinMode(OTA_RST, OUTPUT);
    pinMode(OTA_BOOT0, OUTPUT);
    pinMode(OTA_LED, OUTPUT);

    ESP_LOGI(STM32_OTA_TAG, "UART1 initialized with baud rate: %d", OTA_Baud);
}

String stm32_ota::conect()
{

    String stringtmp;
    int rdtmp;
    delay(100);
    digitalWrite(OTA_BOOT0, HIGH);
    delay(100);
    digitalWrite(OTA_RST, LOW);
    digitalWrite(OTA_LED, LOW);
    delay(50);
    digitalWrite(OTA_RST, HIGH);
    delay(500);
    // for (int i = 0; i < 3; i++)
    // {
    //     digitalWrite(OTA_LED, !digitalRead(OTA_LED));
    //     delay(100);
    // }

    OTA_uart->write(STM32INIT);
    delay(10);
    if (OTA_uart->available() > 0)
        ;
    rdtmp = OTA_uart->read();
    ESP_LOGI(STM32_OTA_TAG, "Init Stm32: 0x:%02x", rdtmp);

    if (rdtmp == STM32ACK)
    {
        stringtmp = STM32_CHIPNAME[GetId()];
    }
    else if (rdtmp == STM32NACK)
    {
        OTA_uart->write(STM32INIT);
        delay(10);
        if (OTA_uart->available() > 0)
            ;
        rdtmp = OTA_uart->read();
        if (rdtmp == STM32ACK)
        {
            stringtmp = STM32_CHIPNAME[GetId()];
        }
    }
    else
        stringtmp = "ERROR";
    return stringtmp;
}

//------------------------------------------------------------------------------
unsigned char stm32_ota::GetId()
{ //
    int getid = 0;
    unsigned char sbuf[5];
    SendCommand(STM32ID);
    while (!OTA_uart->available())
        ;
    sbuf[0] = OTA_uart->read();
    if (sbuf[0] == STM32ACK)
    {
        OTA_uart->readBytesUntil(STM32ACK, sbuf, 4);
        getid = sbuf[1];
        getid = (getid << 8) + sbuf[2];
        if (getid == 0x444)
            return 1;
        if (getid == 0x440)
            return 2;
        if (getid == 0x442)
            return 3;
        if (getid == 0x412)
            return 4;
        if (getid == 0x410)
            return 5;
        if (getid == 0x414)
            return 6;
        if (getid == 0x418)
            return 7;
    }
    else
        return 0;

    return 0;
}
//------------------------------------------------------------------------------
void stm32_ota::SendCommand(unsigned char commd)
{ //
    OTA_uart->write(commd);
    OTA_uart->write(~commd);
}
//----------------------------------------------------------------------------------
unsigned char stm32_ota::Erase()
{ //
    SendCommand(STM32ERASE);
    while (!OTA_uart->available())
        ;
    if (OTA_uart->read() == STM32ACK)
    {
        OTA_uart->write(0xFF);
        OTA_uart->write((uint8_t)0x00);
    }
    else
        return STM32ERR;
    while (!OTA_uart->available())
        ;
    return OTA_uart->read();
}
//------------------------------------------------------------------------------
unsigned char stm32_ota::Erasen()
{ //
    SendCommand(STM32ERASEN);
    while (!OTA_uart->available())
        ;
    if (OTA_uart->read() == STM32ACK)
    {
        OTA_uart->write(0xFF);
        OTA_uart->write(0xFF);
        OTA_uart->write((uint8_t)0x00);
    }
    else
        return STM32ERR;
    while (!OTA_uart->available())
        ;
    return OTA_uart->read();
}
//------------------------------------------------------------------------------
boolean stm32_ota::EraseChip()
{ //
    boolean aux;
    if (Erase() == STM32ACK)
        aux = true;
    else if (Erasen() == STM32ACK)
        aux = true;
    else
        aux = false;
    Sussces = aux;
    return aux;
}
//------------------------------------------------------------------------------
boolean stm32_ota::Flash(String bin_file_name)
{
    File fsUploadFile;
    boolean flashwr;
    int lastbuf = 0;
    uint8_t cflag, fnum = 256;
    int bini = 0;
    uint8_t binread[256];

    fsUploadFile = SPIFFS.open(bin_file_name, "r");
    if (fsUploadFile)
    {
        bini = fsUploadFile.size() / 256;
        lastbuf = fsUploadFile.size() % 256;
        for (int i = 0; i < bini; i++)
        {
            fsUploadFile.read(binread, 256);
            SendCommand(STM32WR);
            while (!OTA_uart->available())
                ;
            cflag = OTA_uart->read();
            if (cflag == STM32ACK)
                if (Address(STM32STADDR + (256 * i)) == STM32ACK)
                {
                    if (SendData(binread, 255) == STM32ACK)
                        flashwr = true;
                    else
                        flashwr = false;
                }
        }
        fsUploadFile.read(binread, lastbuf);
        SendCommand(STM32WR);
        while (!OTA_uart->available())
            vTaskDelay(1);
        cflag = OTA_uart->read();
        if (cflag == STM32ACK)
            if (Address(STM32STADDR + (256 * bini)) == STM32ACK)
            {
                if (SendData(binread, lastbuf) == STM32ACK)
                    flashwr = true;
                else
                    flashwr = false;
            }
        fsUploadFile.close();
    };
    Sussces = flashwr;
    return flashwr;
}

//------------------------------------------------------------------------------
// boolean stm32_ota::downloadFile(String File_Url, String File_Name = "")
// {
//     int StepProgress = 0;
//     int beforeStep = 0;
//     HTTPClient http;
//     WiFiClientSecure client;
//     client.setInsecure();
//     if (File_Name == "")
//     {
//         File_Name = File_Url.substring(File_Url.lastIndexOf("/"), File_Url.length());
//     }
//     else
//     {
//         File_Name = String("/") + File_Name;
//     }

//     bin_file_name = File_Name;
//     http.begin(client, File_Url); // abre o link
//     int httpCode = http.GET();    // verifica se o link e valido
//     if (httpCode > 0)
//     {
//         if (httpCode == HTTP_CODE_OK)
//         { // arquivo OK
//             int len = http.getSize();
//             int paysize = len;
//             uint8_t buff[128] = {0};                       // Cria o buffer para receber os dados
//             WiFiClient *stream = http.getStreamPtr();      // Recebe os dados
//             File configFile = SPIFFS.open(File_Name, "w"); // cria o arquivo na spifss
//             if (!configFile)
//             {
//                 return false;
//             }
//             int Step = paysize / 10;
//             while (http.connected() && (len > 0 || len == -1))
//             {
//                 size_t size = stream->available();
//                 if (size)
//                 {
//                     int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
//                     configFile.write(buff, c);
//                     if (len > 0)
//                     {
//                         len -= c;
//                     }
//                 }
//             }
//             configFile.close(); // finaliza o arquivo
//             http.end();
//             return true;
//         }
//         else
//         {
//             return false;
//         }
//     }
//     else
//     {
//         return false;
//     }
//     return true;
// }
//------------------------------------------------------------------------------
unsigned char stm32_ota::Address(unsigned long addr)
{

    unsigned char sendaddr[4];
    unsigned char addcheck = 0;
    sendaddr[0] = addr >> 24;
    sendaddr[1] = (addr >> 16) & 0xFF;
    sendaddr[2] = (addr >> 8) & 0xFF;
    sendaddr[3] = addr & 0xFF;
    for (int i = 0; i <= 3; i++)
    {
        OTA_uart->write(sendaddr[i]);
        addcheck ^= sendaddr[i];
    }
    OTA_uart->write(addcheck);
    while (!OTA_uart->available())
        ;
    return OTA_uart->read();
}
//------------------------------------------------------------------------------
unsigned char stm32_ota::SendData(unsigned char *data, unsigned char wrlen)
{
    OTA_uart->write(wrlen);
    for (int i = 0; i <= wrlen; i++)
    {
        OTA_uart->write(data[i]);
    }
    OTA_uart->write(getChecksum(data, wrlen));
    while (!OTA_uart->available())
        ;
    return OTA_uart->read();
}
//------------------------------------------------------------------------------
unsigned char stm32_ota::getChecksum(unsigned char *data, unsigned char datalen)
{
    unsigned char lendata = datalen;
    for (int i = 0; i <= datalen; i++)
        lendata ^= data[i];
    return lendata;
}

//----------------------------------------------------------------------------------
void stm32_ota::deletfiles(String filePath)
{
    if (SPIFFS.exists(filePath))
    {
        // Xóa file
        if (SPIFFS.remove(filePath))
        {
            ESP_LOGI("SPIFFS", "File '%s' deleted successfully.", filePath);
        }
        else
        {
            ESP_LOGE("SPIFFS", "Failed to delete file '%s'.", filePath);
        }
    }
    else
    {
        ESP_LOGW("SPIFFS", "File '%s' does not exist.", filePath);
    }
}
//----------------------------------------------------------------------------------
String stm32_ota::binfilename()
{
    String aux = bin_file_name;
    return aux;
}
//----------------------------------------------------------------------------------
void stm32_ota::RunMode()
{ // Tested  Change to runmode
    digitalWrite(OTA_BOOT0, LOW);
    delay(100);
    digitalWrite(OTA_RST, LOW);
    delay(50);
    digitalWrite(OTA_RST, HIGH);
    delay(200);
}
//----------------------------------------------------------------------------------
char stm32_ota::chipVersion()
{ // Tested
    unsigned char vsbuf[14];
    SendCommand(STM32GET);
    while (!OTA_uart->available())
        ;
    vsbuf[0] = OTA_uart->read();
    if (vsbuf[0] != STM32ACK)
        return STM32ERR;
    else
    {
        OTA_uart->readBytesUntil(STM32ACK, vsbuf, 14);
        return vsbuf[1];
    }
}

boolean stm32_ota::FileUpdate(String file_path)
{
    boolean sussces = 0;
    String aux = conect();
    if (aux != "ERROR")
    {
        sussces = EraseChip() && (Flash(file_path));
        RunMode();
    }
    if (sussces)
        ESP_LOGI(STM32_OTA_TAG, "STM32 Update Sussces");
    else
        ESP_LOGI(STM32_OTA_TAG, "STM32 Update Failde");

    return sussces;
}
//----------------------------------------------------------------------------------
// String stm32_ota::otaUpdate(String File_Url)
// {
//     String aux = "";
//     if (WiFi.waitForConnectResult() == WL_CONNECTED)
//     {

//         if (downloadFile(File_Url, "stm32.bin"))
//         {
//             // printfiles();
//             String aux = conect();
//             if (aux != "ERROR")
//             {
//                 EraseChip();
//                 Flash("/stm32.bin");
//                 RunMode();
//                 deletfiles("/stm32.bin");
//             }
//             else
//             {
//                 return "Unknown Chip";
//             }
//         }
//         else
//         {
//             return "Download Fail";
//         }
//     }
//     else
//     {
//         return "WiFi not conected";
//     }
//     return "Update OK";
// }

//----------------------------------------------------------------------------------

String stm32_ota::stm32Read(unsigned long addr, unsigned char n_bytes)
{
    unsigned char rdbuf[15] = {'\0'};
    unsigned char sendaddr[4];
    unsigned char addcheck = 0;
    if (n_bytes > 15)
    {
        n_bytes = 15;
    }
    if (conect() != "ERROR")
    {
        SendCommand(STM32RD);
        while (!OTA_uart->available())
            ;
        if (OTA_uart->read() == STM32ACK)
        {
            sendaddr[0] = addr >> 24;
            sendaddr[1] = (addr >> 16) & 0xFF;
            sendaddr[2] = (addr >> 8) & 0xFF;
            sendaddr[3] = addr & 0xFF;
            for (int i = 0; i <= 3; i++)
            {
                OTA_uart->write(sendaddr[i]);
                addcheck ^= sendaddr[i];
            }
            OTA_uart->write(addcheck);
        }
        else
            return "ERROR";
        while (!OTA_uart->available())
            ;
        if (OTA_uart->read() == STM32ACK)
        {
            OTA_uart->write(n_bytes);
            addcheck = n_bytes ^ STM32XOR;
            OTA_uart->write(addcheck);
        }
        else
            return "ERROR";
        delay(100);
        while (!OTA_uart->available())
            ;
        bytes_buffer = OTA_uart->available();
        OTA_uart->readBytes(rx_buffer, bytes_buffer);
        String myNewString(rx_buffer);
        RunMode();
        return myNewString;
    }
    return "ERROR";
}
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------