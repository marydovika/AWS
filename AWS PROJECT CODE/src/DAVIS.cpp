#include "DAVIS.h"
#include <Arduino.h>

Davis::Davis(){
  lastResetTime = millis();
}

void Davis::setupRainGauge(){
  pinMode(rainfallpin, INPUT);
}

int Davis::readRainGauge(){
  unsigned long currentTime = millis();
  
  // Reset counter every hour (3600000 milliseconds)
  if (currentTime - lastResetTime >= 3600000) {
    rainfallCount = 0;
    lastResetTime = currentTime;
  }
  
  int currentReading = digitalRead(rainfallpin);
  
  // Increment count on transition from HIGH to LOW
  if (lastReading == HIGH && currentReading == LOW) {
    rainfallCount++;
  }
  
  lastReading = currentReading;
  
  // Always return the incremented count
  return rainfallCount;
}
