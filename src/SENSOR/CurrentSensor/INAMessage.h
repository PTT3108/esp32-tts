#pragma once
#include "ArduinoJson.h"
class INAmessage
{
public:
    uint32_t vbus = 0;       // Điện áp VBUS
    int32_t vsum = 0;        // Tổng điện áp
    int32_t current = 0;     // Dòng điện
    uint8_t enery = 0;       // Năng lượng

    INAmessage() {}
    INAmessage(uint32_t vbus, int32_t vsum, int32_t current, uint8_t enery)
        : vbus(vbus), vsum(vsum), current(current), enery(enery) {}

    void serialize(JsonArray &doc)
    {
        // Thêm vbus vào JSON
        JsonObject vbusObj = doc.add<JsonObject>();
        vbusObj["measurement"] = static_cast<float>(vbus) / 1000.0; // Điện áp VBUS (V)
        vbusObj["type"] = "VBUS_Voltage";                          // Loại cảm biến

        // Thêm vsum vào JSON
        JsonObject vsumObj = doc.add<JsonObject>();
        vsumObj["measurement"] = static_cast<float>(vsum) / 1000.0; // Tổng điện áp (V)
        vsumObj["type"] = "Vsum_Voltage";                          // Loại cảm biến

        // Thêm dòng điện vào JSON
        JsonObject currentObj = doc.add<JsonObject>();
        currentObj["measurement"] = static_cast<float>(current) / 1000.0; // Dòng điện (A)
        currentObj["type"] = "Current";                                 // Loại cảm biến

        // Thêm năng lượng vào JSON
        JsonObject energyObj = doc.add<JsonObject>();
        energyObj["measurement"] = static_cast<uint8_t>(enery); // Năng lượng
        energyObj["type"] = "Energy";                          // Loại cảm biến
    }
};
