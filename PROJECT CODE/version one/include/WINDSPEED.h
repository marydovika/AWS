#ifndef WIND_SPEED_H
#define WIND_SPEED_H

#include <Arduino.h>
#define PIN_WIND_SPEED   26

class WindSpeedSensor {
  public:
    WindSpeedSensor();
    void setupSensor();
    float readWindSpeedKPH();
    
    // Interrupt function must be public
    void IRAM_ATTR handleInterrupt();

  private:
    int _pin;
    
    // Volatile variables are modified inside the Interrupt (ISR)
    volatile unsigned long _lastPulseTime;      // Time of the previous valid pulse
    volatile unsigned long _pulseInterval;      // Time between the last two pulses (ms)
    volatile bool _hasNewPulse;                 // Flag to say we got data

    // Constants
    const float KPH_PER_HZ = 4.02336;           // Calibration (2.5mph * 1.609)
    const unsigned long DEBOUNCE_MS = 25;       // Ignore pulses faster than 25ms (max ~100mph)
    const unsigned long TIMEOUT_MS = 3000;      // If no pulse for 3s, wind is 0
};

#endif