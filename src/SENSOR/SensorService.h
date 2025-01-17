#pragma once
class SensorService
{
public:
    static SensorService &getInstance()
    {
        static SensorService instance;
        return instance;
    }
    static bool initialize();
};