#ifdef PLATFORMIO_ESP32
#include <AsyncTCP.h>
#include <WiFi.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#elif defined(TARGET_RP2040)
#include <WebServer.h>
#include <WiFi.h>
#endif
#include "Arduino.h"
#include "WebContent.h"
#include "helps.h"
#include "Web.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "esp_ota_ops.h"
#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include "esp_spi_flash.h"

#include "stm32_ota.h"

static const byte DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress(10, 0, 0, 1); // Địa chỉ IP tĩnh cho Access Point

static AsyncWebServer server(80);

const char *wifi_hostname = "TEST";
const char *wifi_ap_ssid = "TEST DEV";

static bool stm32_seen = false;
#define WIFI_TAG "WIFI DEVICE"

static bool force_update = false;
String Stm32_Target_Found;

static struct
{
    const char *url;
    const char *contentType;
    const uint8_t *content;
    const size_t size;
} files[] = {
    // {"/scan.js", "text/javascript", (uint8_t *)SCAN_JS, sizeof(SCAN_JS)},
    {"/mui.js", "text/javascript", (uint8_t *)MUI_JS, sizeof(MUI_JS)},
    {"/LoraMesh.css", "text/css", (uint8_t *)LoraMesh_CSS, sizeof(LoraMesh_CSS)},
    {"/firmware_update.js", "text/javascript", (uint8_t *)firmware_update_CSS, sizeof(firmware_update_CSS)},
    // {"/hardware.html", "text/html", (uint8_t *)HARDWARE_HTML, sizeof(HARDWARE_HTML)},
    // {"/hardware.js", "text/javascript", (uint8_t *)HARDWARE_JS, sizeof(HARDWARE_JS)},
    // {"/cw.html", "text/html", (uint8_t *)CW_HTML, sizeof(CW_HTML)},
    // {"/cw.js", "text/javascript", (uint8_t *)CW_JS, sizeof(CW_JS)},
};

bool DEV_Wifi::initWiFi()
{
    connected = true;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
#if defined(PLATFORM_ESP8266)
    WiFi.forceSleepBegin();
#endif
    // registerButtonFunction(ACTION_START_WIFI, []()
    //                        { setWifiUpdateMode(); });
    // WiFi.onEvent(WiFiEvent);
    return true;
}

void DEV_Wifi::connectWiFi()
{
    ESP_LOGI(WIFI_TAG, "Switching to AP mode...");
    WiFi.disconnect();
#ifdef PLATFORMIO_ESP32
    WiFi.setHostname(ssid); // hostname must be set before the mode is set to STA
#endif
    WiFi.mode(WIFI_AP);
#ifdef PLATFORM_ESP8266
    WiFi.setHostname(wifi_hostname); // hostname must be set before the mode is set to STA
#endif

#ifdef PLATFORM_ESP8266
    WiFi.setOutputPower(13.5);
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
#elif PLATFORMIO_ESP32
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif

    // Configure AP settings
    WiFi.softAPConfig(ipAddress, ipAddress, netMsk);

    // Start Access Point
    if (WiFi.softAP(ssid, password))
    {
        ESP_LOGI(WIFI_TAG, "AP started with SSID: %s", ssid);
    }
    else
    {
        ESP_LOGE(WIFI_TAG, "Failed to start AP.");
    }

    // Start DN
    // char macStr[5];
    // snprintf(macStr, sizeof(macStr), "%04X", LoraMesher::getInstance().getLocalAddress());
    // Kết hợp SSID với MAC
    // char ap_ssid[50];
    // strncpy(ap_ssid, wifi_ap_ssid, sizeof(ap_ssid) - 1);
    // strncat(ap_ssid, "-", sizeof(wifi_ap_ssid) - strlen(wifi_ap_ssid) - 1);    // Thêm dấu "-"
    // strncat(ap_ssid, macStr, sizeof(wifi_ap_ssid) - strlen(wifi_ap_ssid) - 1); // Thêm MAC
    dnsServer.start(DNS_PORT, "*", ipAddress);
    ESP_LOGI(WIFI_TAG, "DNS server started on IP: %s", ipAddress.toString().c_str());

    startServices();
}
static void WebUpdateHandleRoot(AsyncWebServerRequest *request)
{
    force_update = request->hasArg("force");
    AsyncWebServerResponse *response;
    response = request->beginResponse(200, "text/html", (uint8_t *)INDEX_HTML, sizeof(INDEX_HTML));
    //   if (connectionState == hardwareUndefined)
    //   {
    //     response = request->beginResponse(200, "text/html", (uint8_t *)HARDWARE_HTML, sizeof(HARDWARE_HTML));
    //   }
    //   else
    //   {
    //     response = request->beginResponse(200, "text/html", (uint8_t *)INDEX_HTML, sizeof(INDEX_HTML));
    //   }
    response = request->beginResponse(200, "text/html", (uint8_t *)INDEX_HTML, sizeof(INDEX_HTML));
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);

    // Bắt đầu server
    server.begin();
}

static void WebUpdateSendContent(AsyncWebServerRequest *request)
{
    for (size_t i = 0; i < ARRAY_SIZE(files); i++)
    {
        if (request->url().equals(files[i].url))
        {
            AsyncWebServerResponse *response = request->beginResponse(200, files[i].contentType, files[i].content, files[i].size);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
            return;
        }
    }
    request->send(404, "text/plain", "File not found");
}
static size_t firmwareOffset = 0;

static size_t getFirmwareChunk(uint8_t *data, size_t len, size_t pos)
{
    uint8_t *dst;
    uint8_t alignedBuffer[7];
    if ((uintptr_t)data % 4 != 0)
    {
        // If data is not aligned, read aligned byes using the local buffer and hope the next call will be aligned
        dst = (uint8_t *)((uint32_t)alignedBuffer / 4 * 4);
        len = 4;
    }
    else
    {
        // Otherwise just make sure len is a multiple of 4 and smaller than a sector
        dst = data;
        len = constrain((len / 4) * 4, 4, SPI_FLASH_SEC_SIZE);
    }

    ESP.flashRead(firmwareOffset + pos, (uint32_t *)dst, len);

    // If using local stack buffer, move the 4 bytes into the passed buffer
    // data is known to not be aligned so it is moved byte-by-byte instead of as uint32_t*
    if ((void *)dst != (void *)data)
    {
        for (unsigned b = len; b > 0; --b)
            *data++ = *dst++;
    }
    return len;
}

static void WebUpdateGetFirmware(AsyncWebServerRequest *request)
{
#ifdef PLATFORMIO_ESP32
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running)
    {
        firmwareOffset = running->address;
    }
#endif
    const size_t firmwareTrailerSize = 4096; // max number of bytes for the options/hardware layout json
    AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", (size_t)ESP.getSketchSize() + firmwareTrailerSize, &getFirmwareChunk);
    String filename = String("attachment; filename=\"") + "_" + VERSION + ".bin\"";
    //   String filename = String("attachment; filename=\"") + (const char *)&target_name[4] + "_" + VERSION + ".bin\"";
    response->addHeader("Content-Disposition", filename);
    request->send(response);
}

static void HandleReboot(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Kill -9, no more CPU time!");
    response->addHeader("Connection", "close");
    request->send(response);
    request->client()->close();
}
static void HandleReset(AsyncWebServerRequest *request)
{
    //   if (request->hasArg("hardware")) {
    //     SPIFFS.remove("/hardware.json");
    //   }
    //   if (request->hasArg("options")) {
    //     SPIFFS.remove("/options.json");
    //   }
    //   if (request->hasArg("model") || request->hasArg("config")) {
    //     config.SetDefaults(true);
    //   }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Reset complete, rebooting...");
    response->addHeader("Connection", "close");
    request->send(response);
    request->client()->close();
    //   rebootTime = millis() + 100;
}
static void HandleSTM32Update(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    static File uploadFile;               // Tệp tạm thời để lưu firmware
    static size_t totalBytesReceived = 0; // Tổng số byte đã nhận
    size_t filesize = request->header("X-FileSize").toInt();
    ESP_LOGI(WIFI_TAG, "Update: '%s' size %u", filename.c_str(), filesize);
    if (index == 0) // Bắt đầu tải tệp
    {
        ESP_LOGI(WIFI_TAG, "Starting update: '%s' size %u", filename.c_str(), filesize);

        // Mở tệp trên SPIFFS hoặc LittleFS
        uploadFile = SPIFFS.open("/stm32_firmware.bin", FILE_WRITE);
        if (!uploadFile)
        {
            ESP_LOGE(WIFI_TAG, "Failed to open file for writing");
            return;
        }

        totalBytesReceived = 0;
        stm32_seen = false;
    }

    // Ghi dữ liệu vào tệp
    if (uploadFile)
    {
        uploadFile.write(data, len);
        totalBytesReceived += len;
        ESP_LOGI(WIFI_TAG, "Received %u bytes, total: %u/%u", len, totalBytesReceived, filesize);
    }

    if (final) // Hoàn tất tải tệp
    {
        if (totalBytesReceived == filesize)
        {
            stm32_seen = true;
            ESP_LOGI(WIFI_TAG, "Upload complete: '%s', total size: %u", filename.c_str(), totalBytesReceived);

            // Đóng tệp
            uploadFile.close();

            // Thực hiện cập nhật STM32 từ tệp đã nhận
            // stm32_ota::getInstance().FileUpdate("/stm32_firmware.bin");
            stm32_ota &ota = stm32_ota::getInstance();
            Stm32_Target_Found = ota.conect();
            if (Stm32_Target_Found != "ERROR")
            {
                ota.EraseChip();
                ota.Flash("/stm32_firmware.bin");
                ota.RunMode();
                ota.deletfiles("/stm32_firmware.bin");
            }
            ESP_LOGI("STM32_update", "Uploads info: %s", Stm32_Target_Found);
        }
        else
        {
            ESP_LOGE(WIFI_TAG, "Upload incomplete. Expected: %u, Received: %u", filesize, totalBytesReceived);
            uploadFile.close();
            SPIFFS.remove("/stm32_firmware.bin");
        }
    }
}

static void WebUpdateHandleNotFound(AsyncWebServerRequest *request)
{
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += request->args();
    message += F("\n");

    for (uint8_t i = 0; i < request->args(); i++)
    {
        message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
    }
    AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
}
static void startMDNS()
{
    if (!MDNS.begin(wifi_hostname))
    {
        ESP_LOGE(WIFI_TAG, "Error starting mDNS");
        return;
    }
    String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
    instance.replace(":", "");
#if defined(PLATFORM_ESP8266)
    // We have to do it differently on ESP8266 as setInstanceName has the side-effect of chainging the hostname!
    MDNS.setInstanceName(wifi_hostname);
    MDNSResponder::hMDNSService service = MDNS.addService(instance.c_str(), "http", "tcp", 80);
    MDNS.addServiceTxt(service, "vendor", "test");
    // MDNS.addServiceTxt(service, "target", (const char *)&target_name[4]);
    // MDNS.addServiceTxt(service, "device", (const char *)device_name);
    // MDNS.addServiceTxt(service, "product", (const char *)product_name);
    MDNS.addServiceTxt(service, "version", VERSION);
    // MDNS.addServiceTxt(service, "options", options.c_str());
    MDNS.addServiceTxt(service, "type", "rx");
    // If the probe result fails because there is another device on the network with the same name
    // use our unique instance name as the hostname. A better way to do this would be to use
    // MDNSResponder::indexDomain and change wifi_hostname as well.
    MDNS.setHostProbeResultCallback([instance](const char *p_pcDomainName, bool p_bProbeResult)
                                    {
      if (!p_bProbeResult) {
        WiFi.hostname(instance);
        MDNS.setInstanceName(instance);
      } });
#else
    MDNS.setInstanceName(instance);
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "vendor", "test");
    MDNS.addServiceTxt("http", "tcp", "version", VERSION);
    MDNS.addServiceTxt("http", "tcp", "type", "update");
#endif
}
static void corsPreflightResponse(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse(204, "text/plain");
    request->send(response);
}
static void STM32UpdateResponseHandler(AsyncWebServerRequest *request)
{
    // Giả sử hàm hasError() trả về:
    // 0 => không lỗi, 1 => có lỗi
    int stm32_error = stm32_ota::getInstance().hasError();

    // Trường hợp THÀNH CÔNG là: (stm32_seen == true) VÀ (stm32_error == 0)
    // Trường hợp LỖI là: (stm32_error == 1)  (bỏ qua stm32_seen)
    // Còn lại => mismatch
    if (stm32_seen && (stm32_error == 0))
    {
        // THÀNH CÔNG
        ESP_LOGI(WIFI_TAG, "Update complete, rebooting");

        String msg = "{\"status\": \"ok\", \"msg\": \"Update complete. "
                     "Please wait for a few seconds while the device reboots.\"}";

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", msg);
        response->addHeader("Connection", "close");
        request->send(response);
        request->client()->close();
    }
    else if (stm32_error == 1)
    {
        // LỖI (có lỗi thật sự khi nạp)
        ESP_LOGI(WIFI_TAG, "Failed to upload firmware");
        
        // Tùy ý bạn ghi thêm mô tả lỗi nếu có
        String msg = "{\"status\": \"error\", \"msg\": \"Update failed.\"}";

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", msg);
        response->addHeader("Connection", "close");
        request->send(response);
        request->client()->close();
    }
    else
    {
        // MISMATCH (chưa thấy target STM32 hoặc sai target)
        String message = "{\"status\": \"mismatch\", \"msg\": \"<b>Current target:</b> .<br>";
        if (Stm32_Target_Found != "ERROR")
        {
            message += "<b>Uploaded image:</b> " + Stm32_Target_Found + ".<br/>";
        }
        message += "<br/>It looks like you are flashing firmware with a different name to the current firmware. "
                   "This sometimes happens because the hardware was flashed from the factory with an early version "
                   "that has a different name. Or it may have changed between major releases."
                   "<br/><br/>Please double check you are uploading the correct target, "
                   "then proceed with 'Flash Anyway'.\"}";

        request->send(200, "application/json", message);
    }
}
static void STM32UpdateForceUpdateHandler(AsyncWebServerRequest *request)
{
    stm32_seen = true;
    if (request->arg("action").equals("confirm"))
    {
        STM32UpdateResponseHandler(request);
        request->send(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update cancelled\"}");
    }
    else
    {
        request->send(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update cancelled\"}");
    }
}

void DEV_Wifi::startServices()
{
    server.on("/", WebUpdateHandleRoot);
    server.on("/LoraMesh.css", WebUpdateSendContent);
    server.on("/mui.js", WebUpdateSendContent);
    server.on("/firmware_update.js", WebUpdateSendContent);
    server.on("/firmware.bin", WebUpdateGetFirmware);
    server.on("/update/stm32", HTTP_POST, STM32UpdateResponseHandler, HandleSTM32Update);
    server.on("/update/stm32", HTTP_OPTIONS, corsPreflightResponse);
    server.on("/forceupdate", STM32UpdateForceUpdateHandler);
    server.on("/forceupdate", HTTP_OPTIONS, corsPreflightResponse);

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    server.on("/reboot", HandleReboot);
    server.on("/reset", HandleReset);

    server.onNotFound(WebUpdateHandleNotFound);
    server.begin();

    dnsServer.start(DNS_PORT, "*", ipAddress);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    startMDNS();
}
// void DEV_Wifi::createWiFiTask() {
//     int res = xTaskCreate(
//         wifi_task,
//         "WiFi Task",
//         4096,
//         (void*) 1,
//         2,
//         &wifi_TaskHandle);
//     if (res != pdPASS)
//         ESP_LOGE(TAG, "WiFi task handle error: %d", res);
// }

void DEV_Wifi::HandleWeb()
{
    wl_status_t status = WiFi.status();
    ESP_LOGI(WIFI_TAG, "WiFi status %d", status);
}
