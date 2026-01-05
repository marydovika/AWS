#include <Arduino.h>

#define rainfallpin 25

class Davis{
  public:
    Davis();
    void setupRainGauge();
    int readRainGauge();

  private:
    int rainfallCount = 0;
    unsigned long lastResetTime = 0;
    int lastReading = HIGH;
  
};
