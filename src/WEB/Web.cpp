#ifdef PLATFORMIO_ESP32
#include <AsyncTCP.h>
#include <WiFi.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <soc/uart_pins.h>
#include "Preferences.h"
#include "spi_flash_mmap.h"
#include <mbedtls/sha256.h>
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
#include "common.h"

#include "Web.h"
#include "logging.h"

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "device.h"

#include "stm32_ota.h"
#include "SENSOR/devSensor.h"

#include "ArduinoJson.h"

#include "PWM.h"
static const byte DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress(10, 0, 0, 1); // Địa chỉ IP tĩnh cho Access Point

static AsyncWebServer server(80);

const char *wifi_hostname = "TEST";
const char *wifi_ap_ssid = "TEST DEV";

#define WIFI_TAG "WIFI DEVICE"

static bool target_seen = false;
static uint32_t totalSize;
static bool force_update = false;
String Stm32_Target_Found = "ERROR";

Preferences preferences;

bool deviceStates[5] = {false, false, false, false, false};

void SensorhandleRequest(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    JsonObject dataObj = doc.to<JsonObject>();
    int status = updateSensormessage(dataObj);
    String jsonString;
    serializeJson(doc, jsonString);
    ESP_LOGI("Measuarement", "Sensor: %s", jsonString);
    request->send(200, "application/json", jsonString);
}
static void initDevice()
{
    pinMode(DEVICE_1, OUTPUT);
    pinMode(DEVICE_2, OUTPUT);
    pinMode(DEVICE_3, OUTPUT);
    pinMode(DEVICE_4, OUTPUT);
    pinMode(DEVICE_5, OUTPUT);
}
void updateDeviceStates()
{
    digitalWrite(DEVICE_1, deviceStates[0] ? HIGH : LOW);
    digitalWrite(DEVICE_2, deviceStates[1] ? HIGH : LOW);
    digitalWrite(DEVICE_3, deviceStates[2] ? HIGH : LOW);
    digitalWrite(DEVICE_4, deviceStates[3] ? HIGH : LOW);
    digitalWrite(DEVICE_5, deviceStates[4] ? HIGH : LOW);
}
// Hàm tải trạng thái thiết bị từ NVS
void loadDeviceStates()
{
    ESP_LOGI(WIFI_TAG, "Loading device states from NVS...");
    for (int i = 0; i < 5; i++)
    {
        // Tên key cho mỗi thiết bị
        String key = "device" + String(i);
        // Lấy giá trị từ NVS, nếu không tìm thấy thì giữ nguyên giá trị hiện tại
        deviceStates[i] = preferences.getBool(key.c_str(), deviceStates[i]);
        ESP_LOGI(WIFI_TAG, "Loaded deviceStates[%d] = %s\n", i, deviceStates[i] ? "true" : "false");
    }
    updateDeviceStates();
}

// Hàm lưu trạng thái thiết bị vào NVS
void saveDeviceStates()
{
    for (int i = 0; i < 5; i++)
    {
        // Tên key cho mỗi thiết bị
        String key = "device" + String(i);
        // Lưu giá trị vào NVS
        preferences.putBool(key.c_str(), deviceStates[i]);
        // ESP_LOGI(WIFI_TAG,"Saved deviceStates[%d] = %s\n", i, deviceStates[i] ? "true" : "false");
    }
    updateDeviceStates();
}

String loadFilenameFromNVS()
{
    ESP_LOGI(WIFI_TAG, "Loading filename from NVS...");
    // Key cho filename
    const char *filenameKey = "filename";

    // Đọc chuỗi từ NVS, nếu không tìm thấy thì trả về chuỗi rỗng
    String filename = preferences.getString(filenameKey, "");

    if (filename.length() > 0)
    {
        ESP_LOGI(WIFI_TAG, "Loaded filename: %s\n", filename.c_str());
    }
    else
    {
        ESP_LOGI(WIFI_TAG, "No filename found in NVS.");
    }

    return filename;
}

void saveFilenameToNVS(const String &filename)
{
    ESP_LOGI(WIFI_TAG, "Saving filename to NVS...");
    // Key cho filename
    const char *filenameKey = "filename";

    // Lưu chuỗi vào NVS
    preferences.putString(filenameKey, filename);

    ESP_LOGI(WIFI_TAG, "Saved filename: %s\n", filename.c_str());
}
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

// Hàm cập nhật NVS
void savePWMConfigToNVS(uint32_t freq1, uint8_t dir1,
                        uint32_t freq2, uint8_t dir2,
                        uint32_t freq3, uint8_t dir3)
{
    preferences.putUInt("freq1", freq1);
    preferences.putUChar("dir1", dir1);

    preferences.putUInt("freq2", freq2);
    preferences.putUChar("dir2", dir2);

    preferences.putUInt("freq3", freq3);
    preferences.putUChar("dir3", dir3);

    ESP_LOGI("Stepper NVS", "PWM config saved to NVS");
    pwm.set_pwm(128, 127, freq1);
    WRITE(129, dir1);
    pwm.set_pwm(130, 127, freq2);
    WRITE(131, dir1);
    pwm.set_pwm(132, 127, freq3);
    WRITE(133, dir1);
}

// Hàm load NVS + áp dụng
void loadPWMConfigFromNVS()
{
    // Mặc định tần số 1000,1500,2000 Hz, dir = 0
    uint32_t freq1 = preferences.getUInt("freq1", 1000);
    uint8_t dir1 = preferences.getUChar("dir1", 0);

    uint32_t freq2 = preferences.getUInt("freq2", 1500);
    uint8_t dir2 = preferences.getUChar("dir2", 0);

    uint32_t freq3 = preferences.getUInt("freq3", 2000);
    uint8_t dir3 = preferences.getUChar("dir3", 0);

    ESP_LOGI("Stepper NVS", "Loaded PWM config from NVS:");
    ESP_LOGI("Stepper NVS", "CH1: freq=%u, dir=%u\n", freq1, dir1);
    ESP_LOGI("Stepper NVS", "CH2: freq=%u, dir=%u\n", freq2, dir2);
    ESP_LOGI("Stepper NVS", "CH3: freq=%u, dir=%u\n", freq3, dir3);
    pwm.set_pwm(128, 127, freq1);
    WRITE(129, dir1);
    pwm.set_pwm(130, 127, freq2);
    WRITE(131, dir1);
    pwm.set_pwm(132, 127, freq3);
    WRITE(133, dir1);
}

void handlePwmUpdate(AsyncWebServerRequest *request)
{
    int params = request->params();
    static uint32_t lastUpdatePwm;
    if (lastUpdatePwm - millis() <= 1000)
    {
        request->send(400, "application/json", "{\"error\":\"Update Pwm fast\"}");
        return;
    }

    // Kiểm tra dữ liệu JSON trong body
    for (int i = 0; i < params; i++)
    {
        const AsyncWebParameter *p = request->getParam(i);
        ESP_LOGI("sacsac", "_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
    if (request->hasArg("plain"))
    {
        String jsonBody = request->arg("plain"); // Chuỗi JSON

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonBody);
        if (err)
        {
            // JSON không hợp lệ
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        serializeJson(doc,Serial);

        // Lấy freq và dir cho từng channel
        // Dùng toán tử | (hoặc) như một cách đặt giá trị mặc định
        uint32_t freq1 = doc["channel1"]["freq"] | 1000;
        uint8_t dir1 = doc["channel1"]["dir"] | 0;

        uint32_t freq2 = doc["channel2"]["freq"] | 1500;
        uint8_t dir2 = doc["channel2"]["dir"] | 0;

        uint32_t freq3 = doc["channel3"]["freq"] | 2000;
        uint8_t dir3 = doc["channel3"]["dir"] | 0;

        // 2. Lưu vào NVS
        savePWMConfigToNVS(freq1, dir1, freq2, dir2, freq3, dir3);

        // 3. Gửi phản hồi
        String response = "{\"status\":\"ok\",\"ch1\":{\"freq\":" + String(freq1) +
                          ",\"dir\":" + String(dir1) +
                          "},\"ch2\":{\"freq\":" + String(freq2) +
                          ",\"dir\":" + String(dir2) +
                          "},\"ch3\":{\"freq\":" + String(freq3) +
                          ",\"dir\":" + String(dir3) + "}}";

        request->send(200, "application/json", response);
    }
    else
    {
        // Không có "plain" => thiếu body
        request->send(400, "application/json", "{\"error\":\"No JSON body\"}");
    }
}
void handleGetPwmConfig(AsyncWebServerRequest *request)
{
    // Đọc cấu hình PWM/dir từ NVS, hoặc giá trị mặc định nếu chưa có
    uint32_t freq1 = preferences.getUInt("freq1", 0);
    uint8_t dir1 = preferences.getUChar("dir1", 0);

    uint32_t freq2 = preferences.getUInt("freq2", 0);
    uint8_t dir2 = preferences.getUChar("dir2", 0);

    uint32_t freq3 = preferences.getUInt("freq3", 0);
    uint8_t dir3 = preferences.getUChar("dir3", 0);

    // Dựng JSON trả về
    JsonDocument doc;
    doc["channel1"]["freq"] = freq1;
    doc["channel1"]["dir"] = dir1;
    doc["channel2"]["freq"] = freq2;
    doc["channel2"]["dir"] = dir2;
    doc["channel3"]["freq"] = freq3;
    doc["channel3"]["dir"] = dir3;

    String response;
    serializeJson(doc, response);

    // Gửi phản hồi 200 OK, nội dung JSON
    request->send(200, "application/json", response);
}

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

    if (!preferences.begin("storage", false))
    {
        ESP_LOGI(WIFI_TAG, "Failed to initialize Preferences");
    }
    else
    {
        ESP_LOGI(WIFI_TAG, "Preferences initialized");
    }

    // Tải trạng thái thiết bị từ NVS
    loadDeviceStates();
    initDevice();
    updateDeviceStates();
    loadPWMConfigFromNVS();
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

uint32_t updateCRC32(uint32_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void HandleSTM32Update(AsyncWebServerRequest *request,
                              const String &filename,
                              size_t index,
                              uint8_t *data,
                              size_t len,
                              bool final)
{
    static File uploadFile;               // Tệp tạm thời để lưu firmware
    static size_t totalBytesReceived = 0; // Tổng số byte đã nhận
    static uint32_t firmwareCRC = 0;

    // Lấy dung lượng file mong đợi từ header
    size_t filesize = request->header("X-FileSize").toInt();

    if (index == 0)
    {
        // Bắt đầu upload
        ESP_LOGI(WIFI_TAG, "Starting update: '%s' size=%u", filename.c_str(), filesize);

        uploadFile = SPIFFS.open(STM32_FRIMWARE_PATH, FILE_WRITE);
        if (!uploadFile)
        {
            ESP_LOGE(WIFI_TAG, "Failed to open file for writing");
            return; // Không thể ghi file -> thoát luôn
        }

        // Khởi tạo biến CRC với giá trị 0xFFFFFFFF (theo chuẩn CRC-32 IEEE)
        firmwareCRC = 0xFFFFFFFF;
        totalBytesReceived = 0;
    }

    // Ghi dữ liệu vào file nếu còn mở
    if (uploadFile)
    {
        uploadFile.write(data, len);
        totalBytesReceived += len;

        // Cập nhật CRC
        firmwareCRC = updateCRC32(firmwareCRC, data, len);

        ESP_LOGI(WIFI_TAG, "Received %u bytes, total: %u/%u",
                 len, totalBytesReceived, filesize);
    }

    // Nếu là chunk cuối (final == true)
    if (final)
    {
        // Đảo CRC để lấy giá trị cuối
        firmwareCRC ^= 0xFFFFFFFF;

        // Đóng file (quan trọng: nên đóng file trước khi xóa hay dùng tiếp)
        if (uploadFile)
        {
            uploadFile.close();
        }

        // Kiểm tra số byte đã nhận
        if (totalBytesReceived == filesize)
        {
            ESP_LOGI(WIFI_TAG, "Upload complete: '%s', total size: %u",
                     filename.c_str(), totalBytesReceived);

            // In CRC tính được
            ESP_LOGI(WIFI_TAG, "Calculated CRC-32: 0x%08X", firmwareCRC);

            // Đọc CRC mong đợi từ header (nếu client gửi)
            String expectedCRCHeader = request->header("X-CRC32");

            if (!expectedCRCHeader.isEmpty())
            {
                // Chuyển từ chuỗi hex sang số
                uint32_t expectedCRC = strtoul(expectedCRCHeader.c_str(), NULL, 16);
                ESP_LOGI(WIFI_TAG, "Expected CRC-32 from header: 0x%08X", expectedCRC);

                // So sánh
                if (firmwareCRC == expectedCRC)
                {
                    ESP_LOGI(WIFI_TAG, "CRC OK, proceed with firmware update.");

                    // Nếu CRC khớp -> ta có thể tiến hành update firmware
                    saveFilenameToNVS(filename);
                    devicesTriggerEvent(EVENT_UPDATE_FIRMWARE_STM32);
                }
                else
                {
                    // Nếu CRC không khớp -> xóa file, báo lỗi
                    ESP_LOGE(WIFI_TAG, "CRC mismatch! Removing the file.");
                    SPIFFS.remove(STM32_FRIMWARE_PATH);
                }
            }
            else
            {
                ESP_LOGW(WIFI_TAG, "No 'X-CRC32' header. Skip CRC check or handle error.");
                saveFilenameToNVS(filename);
                devicesTriggerEvent(EVENT_UPDATE_FIRMWARE_STM32);
            }
        }
        else
        {
            // Trường hợp không nhận đủ dữ liệu
            ESP_LOGE(WIFI_TAG,
                     "Upload incomplete. Expected: %u bytes, Received: %u bytes",
                     filesize, totalBytesReceived);

            // Xóa file do chưa nhận đủ
            SPIFFS.remove(STM32_FRIMWARE_PATH);
        }
    }
}

static void STM32UpdateResponseHandler(AsyncWebServerRequest *request)
{
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (request->header("X-FileSize").toInt() == 0)
    {
        Stm32_Target_Found = "ERROR";
        devicesTriggerEvent(EVENT_UPDATE_FIRMWARE_STM32);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (Stm32_Target_Found != "ERROR")
    {
        String msg;
        // THÀNH CÔNG
        ESP_LOGI(WIFI_TAG, "Webresponse ");

        msg = String("{\"status\": \"ok\", \"msg\": \"Update complete. ");
        msg += "Please wait for a few seconds while the device " + Stm32_Target_Found + " reboots.\"}";

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
        response->addHeader("Connection", "close");
        request->send(response);
        Stm32_Target_Found = "ERROR";
    }
    else
    {
        // LỖI (có lỗi thật sự khi nạp)
        ESP_LOGI(WIFI_TAG, "Failed to upload firmware");

        // Tùy ý bạn ghi thêm mô tả lỗi nếu có
        String msg = "{\"status\": \"error\", \"msg\": \"Update failed " + Stm32_Target_Found + ".\"}";

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
        response->addHeader("Connection", "close");
        request->send(response);
    }
}

static void STM32UpdateForceUpdateHandler(AsyncWebServerRequest *request)
{
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
static void WebUploadResponseHandler(AsyncWebServerRequest *request)
{
    if (target_seen || Update.hasError())
    {
        String msg;
        if (!Update.hasError() && Update.end())
        {
            ESP_LOGI(WIFI_TAG, "Update complete, rebooting");
            msg = String("{\"status\": \"ok\", \"msg\": \"Update complete. ");
#if defined(TARGET_RX)
            msg += "Please wait for the LED to resume blinking before disconnecting power.\"}";
#else
            msg += "Please wait for a few seconds while the device reboots.\"}";
#endif
        }
        else
        {
            StreamString p = StreamString();
            if (Update.hasError())
            {
                Update.printError(p);
            }
            else
            {
                p.println("Not enough data uploaded!");
            }
            p.trim();
            ESP_LOGI(WIFI_TAG, "Failed to upload firmware: %s", p.c_str());
            msg = String("{\"status\": \"error\", \"msg\": \"") + p + "\"}";
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
        response->addHeader("Connection", "close");
        request->send(response);
        request->client()->close();
    }
    else
    {
        String message = String("{\"status\": \"mismatch\", \"msg\": \"<b>Current target:</b> ") + ".<br>";
        message += "<br/>It looks like you are flashing firmware with a different name to the current  firmware.  This sometimes happens because the hardware was flashed from the factory with an early version that has a different name. Or it may have even changed between major releases.";
        message += "<br/><br/>Please double check you are uploading the correct target, then proceed with 'Flash Anyway'.\"}";
        request->send(200, "application/json", message);
    }
}

static void WebUploadDataHandler(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    force_update = force_update || request->hasArg("force");
    if (index == 0)
    {
#if defined(TARGET_TX) && defined(PLATFORM_ESP32)
        WifiJoystick::StopJoystickService();
#endif

        size_t filesize = request->header("X-FileSize").toInt();
        ESP_LOGI(WIFI_TAG, "Update: '%s' size %u", filename.c_str(), filesize);
#if defined(PLATFORM_ESP8266)
        Update.runAsync(true);
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        ESP_LOGI(WIFI_TAG, "Free space = %u", maxSketchSpace);
        UNUSED(maxSketchSpace); // for warning
#endif
        if (!Update.begin(filesize, U_FLASH))
        { // pass the size provided
            Update.printError(Serial);
        }
        target_seen = false;
        totalSize = 0;
    }
    if (len)
    {
        ESP_LOGI(WIFI_TAG, "Writing %d bytes of firmware data.", len);

        // Ghi dữ liệu vào bộ nhớ flash
        if (Update.write(data, len) == len)
        {
            // Vì force_update luôn bằng true, target_seen luôn là true
            target_seen = true; // Có thể loại bỏ nếu không sử dụng ở nơi khác

            // Cập nhật tổng kích thước đã ghi
            totalSize += len;

            // Ghi log thông báo ghi thành công
            ESP_LOGI(WIFI_TAG, "Successfully wrote %d bytes. Total written: %u bytes.", len, totalSize);
        }
        else
        {
            ESP_LOGE(WIFI_TAG, "Failed to write %d bytes of firmware data.", len);
        }
    }
}

void DEV_Wifi::startServices()
{
    server.on("/", WebUpdateHandleRoot);
    server.on("/LoraMesh.css", WebUpdateSendContent);
    server.on("/mui.js", WebUpdateSendContent);
    server.on("/firmware_update.js", WebUpdateSendContent);
    server.on("/firmware.bin", WebUpdateGetFirmware);

    server.on("/update/esp32", HTTP_POST, WebUploadResponseHandler, WebUploadDataHandler);
    server.on("/update/esp32", HTTP_OPTIONS, corsPreflightResponse);
    server.on("/update/stm32", HTTP_POST, STM32UpdateResponseHandler, HandleSTM32Update);
    server.on("/update/stm32", HTTP_OPTIONS, corsPreflightResponse);
    server.on("/forceupdate", STM32UpdateForceUpdateHandler);
    server.on("/forceupdate", HTTP_OPTIONS, corsPreflightResponse);

    server.on("/measurement", HTTP_GET, SensorhandleRequest);
    server.on("/stepperUpdate", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    // Truyền sang hàm handlePwmUpdate
    handlePwmUpdate(request); });

    server.on("/getStepperConfig", HTTP_GET, handleGetPwmConfig);

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    server.on("/reboot", HandleReboot);
    server.on("/reset", HandleReset);

    server.on("/getStates", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    String json = "[";
    for (int i = 0; i < 5; i++) {
      json += deviceStates[i] ? "true" : "false";
      if (i < 4) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json); });

    // API: Thay đổi trạng thái thiết bị
    server.on("/setState", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                ESP_LOGI("setStatast","SETETET");
    if (request->hasParam("index", false)) {
      int index = request->getParam("index", false)->value().toInt();
      if (index >= 1 && index <= 5) {
        deviceStates[index - 1] = !deviceStates[index - 1]; // Đảo trạng thái thiết bị
        saveDeviceStates();

        request->send(200, "application/json", "{\"message\":\"Device state toggled!\"}");
      } else {
        request->send(400, "application/json", "{\"message\":\"Invalid device index!\"}");
      }
    } else {
      request->send(400, "application/json", "{\"message\":\"Missing device index!\"}");
    } });

    server.on("/getversionfrimware", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                    String filename = loadFilenameFromNVS();
    
    // Tạo phản hồi JSON
    String jsonResponse = "{\"filename\":\"" + filename + "\"}";
    
    // Gửi phản hồi
    request->send(200, "application/json", jsonResponse); });

    server.onNotFound(WebUpdateHandleNotFound);
    server.begin();

    dnsServer.start(DNS_PORT, "*", ipAddress);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    startMDNS();
}

void DEV_Wifi::HandleWeb()
{
    wl_status_t status = WiFi.status();
    ESP_LOGI(WIFI_TAG, "WiFi status %d", status);
}
