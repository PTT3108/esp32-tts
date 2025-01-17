#pragma once

#include <Arduino.h>
#include "ArduinoJson.h"
#pragma pack(1)
class PHSensorMessage
{
public:
    float temperature;
    float ph;

    PHSensorMessage() {}

    PHSensorMessage(float temperature, float ph) : temperature(temperature), ph(ph) {}
    void serialize(JsonArray &doc)
    {
        // Thêm nhiệt độ vào JSON
        JsonObject tempObj = doc.add<JsonObject>();
        tempObj["measurement"] = static_cast<float>(temperature) / 100.0; // Nhiệt độ (°C)
        tempObj["type"] = "PH_Temperature";                             // Loại cảm biến

        // Thêm áp suất vào JSON
        JsonObject pressureObj = doc.add<JsonObject>();
        pressureObj["measurement"] = static_cast<float>(ph) / 10.0; // Áp suất (hPa)
        pressureObj["type"] = "PH";                            // Loại cảm biến
    }
};
#pragma pack()