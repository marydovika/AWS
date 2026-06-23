#ifndef DAVIS_H
#define DAVIS_H

#include <Arduino.h>

class Davis {
  public:
    Davis();
    void setupRainGauge();
    int readRainGauge();

    // This must be public so the ISR can access it
    void IRAM_ATTR handleInterrupt();

  private:
    // UPDATE THIS PIN to match your wiring
    const int rainfallpin = 25; 

    // Volatile is crucial for variables changed in an interrupt
    volatile unsigned int rainCounter; 
    volatile unsigned long lastDebounceTime;

    unsigned long lastResetTime;
    
    // 200ms debounce to prevent double-counting a single tip
    const unsigned long DEBOUNCE_TIME = 200; 
    const unsigned long RESET_INTERVAL = 3600000; // 1 Hour in ms
};

#endif