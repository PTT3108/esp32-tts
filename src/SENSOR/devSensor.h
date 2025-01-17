#pragma once

#include "device.h"
#include "common.h"
#include "device.h"
#include "ArduinoJson.h"
#if defined(USE_ANALOG_VBAT)
#include "devAnalogVbat.h"
#endif
enum eBaroReadState : uint8_t
{
    brsNoBaro,
    brsUninitialized,
    brsReadTemp,
    brsWaitingTemp,
    brsReadPres,
    brsWaitingPress
};

extern device_t Sensor_dev;
int updateSensormessage(JsonObject &doc);