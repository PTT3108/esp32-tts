#pragma once

#include <stdint.h>
#include <stddef.h>
#include "ArduinoJson.h"
class BaroMessage
{
public:
    uint32_t pressure = 0;
    int32_t temperature = -273;
    BaroMessage() {}
    BaroMessage(uint32_t pressure, int32_t temperature) : pressure(pressure), temperature(temperature) {}
    void serialize(JsonArray &doc)
    {
        // Thêm nhiệt độ vào JSON
        JsonObject tempObj = doc.add<JsonObject>();
        tempObj["measurement"] = static_cast<float>(temperature) / 100.0; // Nhiệt độ (°C)
        tempObj["type"] = "Baro_Temperature";                          // Loại cảm biến

        // Thêm áp suất vào JSON
        JsonObject pressureObj = doc.add<JsonObject>();
        pressureObj["measurement"] = static_cast<float>(pressure) / 10.0; // Áp suất (hPa)
        pressureObj["type"] = "Baro_Pressure";                               // Loại cảm biến
    }
};
class BaroBase
{
public:
    static const int32_t ALTITUDE_INVALID = 0x7fffffff;
    static const int32_t TEMPERATURE_INVALID = 0x7fffffff;
    static const uint32_t PRESSURE_INVALID = 0xffffffff;
    BaroBase() : m_initialized(false), m_altitudeHome(ALTITUDE_INVALID) {}

    virtual void initialize() = 0;
    // // Return expected duration of pressure measurement (ms)
    virtual uint8_t getPressureDuration() = 0;
    // Start reading pressure
    virtual void startPressure() = 0;
    // Get pressure result (deci-Pascals)
    virtual uint32_t getPressure() = 0;
    // Return expected duration of temperature measurement (ms)
    virtual uint8_t getTemperatureDuration() = 0;
    // Start reading temperature
    virtual void startTemperature() = 0;
    // Get temperature result (centiDegrees)
    virtual int32_t getTemperature() = 0;

    // Base functions
    bool isInitialized() const { return m_initialized; }
    int32_t pressureToAltitude(uint32_t pressurePa);
    // Properties
    int32_t getAltitudeHome() const { return m_altitudeHome; }
    void setAltitudeHome(int32_t altitudeHome) { m_altitudeHome = altitudeHome; }
    BaroMessage read()
    {
        return BaroMessage(m_pressureLast, m_temperature);
    };

protected:
    uint32_t m_pressureLast = 0;
    int32_t m_temperature = -273;
    bool m_initialized;
    int32_t m_altitudeHome;
};

class BaroI2CBase : public BaroBase
{
protected:
    static uint8_t m_address;
    static void readRegister(uint8_t reg, uint8_t *data, size_t size);
    static void writeRegister(uint8_t reg, uint8_t *data, size_t size);
};
