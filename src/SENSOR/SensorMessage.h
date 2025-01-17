#include "PHSensor/PHSensor.h"
#include "Baro/baro_base.h"
#include "CurrentSensor/INAMessage.h"
#include "ArduinoJson.h"
class MeasurementMessage
{
public:
    PHSensorMessage phSensorMessage;
    BaroMessage baro;
    INAmessage ina;
    void serialize(JsonObject &doc)
    {
        // Create a data object
        JsonObject dataObj = doc["data"].to<JsonObject>();

        // Add the data to the data object
        serializeDataSerialize(dataObj);
    }
    void serializeDataSerialize(JsonObject &doc)
    {
        // // Call the base class serialize function
        // ((DataMessageGeneric *)(this))->serialize(doc);

        // Add the GPS data to the JSON object
        // gps.serialize(doc);

        // Add that is a measurement message
        doc["message_type"] = "measurement";

        // Set all the measurements in an array called "message"
        JsonArray measurements = doc["message"].to<JsonArray>(); // Thay createNestedArray bằng to<JsonArray>()

        // // Add the PH sensor data to the JSON object
        phSensorMessage.serialize(measurements);
        baro.serialize(measurements);   
        ina.serialize(measurements);            

    }
};