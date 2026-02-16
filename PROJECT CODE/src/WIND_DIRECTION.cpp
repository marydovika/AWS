#include "WIND_DIRECTION.h"

// Define the pin here as requested
#define PIN_WIND_DIR 27

WindDirectionSensor::WindDirectionSensor() {
  _pin = PIN_WIND_DIR;
  _windDirectionDeg = 0;
  _smoothedADC = 0;
}

void WindDirectionSensor::setupSensor() {
  // Configures the pin to read full voltage range (approx 0-3.3V)
  analogSetPinAttenuation(_pin, ADC_11db); 
  
  // Take one initial reading so the average doesn't start at 0
  _smoothedADC = analogRead(_pin);
}

int WindDirectionSensor::readWindDirectionDeg() {
  // 1. Read the raw value instantly (No delay!)
  int rawReading = analogRead(_pin);
  
  // 2. Apply "Exponential Moving Average" filter
  // This replaces the 'for loop' and 'delay'.
  // It mathematically smooths the jitter every time you call this function.
  _smoothedADC = (rawReading * ALPHA) + (_smoothedADC * (1.0 - ALPHA));

  // 3. Map the smoothed value to degrees
  // REMINDER: Ensure your voltage divider reduces the sensor's 5V output to max 3.3V
  _windDirectionDeg = map((long)_smoothedADC, 0, ADC_RESOLUTION, 0, MAX_ANGLE);

  // Boundary check
  if (_windDirectionDeg >= 360) _windDirectionDeg = 0;

  return _windDirectionDeg;
}