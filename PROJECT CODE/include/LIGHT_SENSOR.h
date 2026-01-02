#pragma once

class LightSensor {
public:
    LightSensor();
    float readLightLevel(); 
    void setupSensor();  
private:
    int _pin;
    float light_voltage_;
};