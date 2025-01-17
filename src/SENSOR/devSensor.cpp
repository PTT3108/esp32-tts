#include "devSensor.h"
#include "CurrentSensor/INA219.h"
#include "logging.h"
#include "common.h"
#include "Wire.h"
#include "./Baro/baro_bmp280.h"
#include "./Baro/baro_spl06.h"
#include "SensorService.h"
#include "SensorMessage.h"
#define BARO_STARTUP_INTERVAL 100

static BaroBase *baro;
static eBaroReadState BaroReadState;
static INA219 INA(0x40);
size_t sensorMessageId = 0;
#include "PHSensor/PHSensor.h"
PHSensor *phSensor = new PHSensor();

int updateSensormessage(JsonObject &doc)
{
    MeasurementMessage *message = new MeasurementMessage();
    JsonObject dataObj = doc["data"].to<JsonObject>();
    message->phSensorMessage = phSensor->read();
    message->baro = baro->read();
    message->ina = INA.read();
    message->serialize(dataObj);
    delete message;
    return 22000;
}
static bool Baro_Detect()
{
    // I2C Baros

    if (SPL06::detect())
    {
        DBGLN("Detected baro: SPL06");
        baro = new SPL06();
        // return true;
    }
    if (BMP280::detect())
    {
        DBGLN("Detected baro: BMP280");
        baro = new BMP280();
    }
    return true;

    DBGLN("Detected baro: NONE");
} // I2C
static int Baro_Init()
{
    baro->initialize();
    if (baro->isInitialized())
    {
        // Slow down Vbat updates to save bandwidth
        BaroReadState = brsReadTemp;
        return DURATION_IMMEDIATELY;
    }

    // Did not init, try again later
    return BARO_STARTUP_INTERVAL;
}

bool SensorService::initialize()
{
    Wire.begin(41, 40);
    // devCurrent = new devINA219();
    if (!INA.begin())
    {
        DBGLN("INA219 init false");
    }
    DBGLN("INA219 init true");
    return 1;
}

static int stat()
{
    if (Baro_Detect())
    {
        BaroReadState = brsUninitialized;
        return BARO_STARTUP_INTERVAL;
    }

    BaroReadState = brsNoBaro;
    return DURATION_NEVER;
}

static int timeout()
{
    // DBGLN("Current INA219 %f", INA.getBusVoltage_mV());
    switch (BaroReadState)
    {
    default: // fallthrough
    case brsNoBaro:
        return DURATION_NEVER;

    case brsUninitialized:
        return Baro_Init();

    case brsWaitingTemp:
    {
        int32_t temp = baro->getTemperature();
        if (temp == BaroBase::TEMPERATURE_INVALID)
            return 3000;
    }
        // fallthrough

    case brsReadPres:
    {
        uint8_t pressDuration = baro->getPressureDuration();
        BaroReadState = brsWaitingPress;
        if (pressDuration != 0)
        {
            baro->startPressure();
            return pressDuration;
        }
    }
        // fallthrough

    case brsWaitingPress:
    {
        uint32_t press = baro->getPressure();
        if (press == BaroBase::PRESSURE_INVALID)
            return 3000;
        // Baro_PublishPressure(press);
    }
        // fallthrough

    case brsReadTemp:
    {
        uint8_t tempDuration = baro->getTemperatureDuration();
        if (tempDuration == 0)
        {
            BaroReadState = brsReadPres;
            return 3000;
        }
        BaroReadState = brsWaitingTemp;
        baro->startTemperature();
        return tempDuration;
    }
    }
}

static int sensorsenevent()
{
    return DURATION_NEVER;
}
device_t Sensor_dev = {
    .initialize = SensorService::getInstance().initialize,
    .start = stat,
    .event = nullptr,
    .timeout = timeout,
    .subscribe = EVENT_NONE};