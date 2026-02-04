#ifndef WIND_DIRECTION_H
#define WIND_DIRECTION_H

#include <Arduino.h>

class WindDirectionSensor {
  public:
    WindDirectionSensor();
    void setupSensor();
    int readWindDirectionDeg();

  private:
    int _pin;
    int _windDirectionDeg;
    
    // Variable to hold the running average (Float for precision)
    float _smoothedADC; 

    // Constants
    const int ADC_RESOLUTION = 4095; // ESP32 12-bit ADC
    const int MAX_ANGLE = 360;
    
    // Smoothing Factor (0.0 to 1.0)
    // 0.1 means we trust the new reading 10% and the old average 90%.
    // This creates a smooth needle movement without using delay().
    const float ALPHA = 0.1; 
};

#endif