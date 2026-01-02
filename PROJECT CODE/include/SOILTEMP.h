#pragma once

class SoilTemp {
public:
    SoilTemp();
    void sensor_setup(); // initialize the sensor
    float readSoilTemperature(); // read soil temperature in Celsius
private:
    float soil_temperature_;
};