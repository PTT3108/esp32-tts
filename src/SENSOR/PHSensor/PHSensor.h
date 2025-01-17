#pragma once

#include <Arduino.h>

#include <Wire.h> //I2C Arduino Library


#include "PHSensorMessage.h"

class PHSensor {
public:
    void init();

    PHSensorMessage read();

private:
    bool initialized = false;
};

