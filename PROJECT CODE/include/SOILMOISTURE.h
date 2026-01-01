#pragma once

class Soilmoisture {
public:
    Soilmoisture();
    float readSoilMoisture(); 
    int digitizeSoilMoisture();
    void setupSensor();  
private:
    int _pin;
    float moisture_voltage_;
    int digital_value_;
};