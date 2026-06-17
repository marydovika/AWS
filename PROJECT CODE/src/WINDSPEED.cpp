#include "WINDSPEED.h"
#include "ERROR_LOGGER.h"

// Pointer for the global ISR
WindSpeedSensor* speedSensorInstance = nullptr;

// Flag set inside the ISR if it fires before speedSensorInstance is ready.
// Checked and logged later in readWindSpeedKPH() since SD/Serial I/O
// is not safe to call directly from interrupt context.
volatile bool g_windIsrNullFlag = false;

// Global ISR wrapper
void IRAM_ATTR speedGlobalISR() {
  if (speedSensorInstance) {
    speedSensorInstance->handleInterrupt();
  } else {
    g_windIsrNullFlag = true;
  }
}

WindSpeedSensor::WindSpeedSensor() {
  _pin = PIN_WIND_SPEED;
  _lastPulseTime = 0;
  _pulseInterval = 0;
  _hasNewPulse = false;
  speedSensorInstance = this;
}

void WindSpeedSensor::setupSensor() {
  pinMode(_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(_pin), speedGlobalISR, FALLING);
}

// ---------------------------------------------------------
// NEW LOGIC: Measures time between clicks (Period) 
// instead of counting clicks (Frequency)
// ---------------------------------------------------------
void IRAM_ATTR WindSpeedSensor::handleInterrupt() {
  unsigned long currentTime = millis();
  unsigned long timeDiff = currentTime - _lastPulseTime;

  // Debounce: Only accept pulse if enough time passed since last one
  // 25ms limit prevents the "32km/h" bounce noise
  if (timeDiff > DEBOUNCE_MS) {
    _pulseInterval = timeDiff;       // Store how long this rotation took
    _lastPulseTime = currentTime;    // Reset timer
    _hasNewPulse = true;             // Tell main loop we have a reading
  }
}

float WindSpeedSensor::readWindSpeedKPH() {
  unsigned long currentTime = millis();

  // Check for ISR-before-init flag set from interrupt context
  if (g_windIsrNullFlag) {
    ErrorLogger::log(COMP_WINDSPEED, ERR_WIND_ISR_NULL, "ISR fired before speedSensorInstance was set");
    g_windIsrNullFlag = false;
  }

  // 1. TIMEOUT CHECK
  // If we haven't seen a pulse in 3 seconds, assume wind stopped.
  if ((currentTime - _lastPulseTime) > TIMEOUT_MS) {
    ErrorLogger::log(COMP_WINDSPEED, ERR_WIND_TIMEOUT, "No pulse for 3s - returning 0.0 (calm or sensor fault)");
    return 0.0;
  }

  // 2. CALCULATE SPEED
  // If we have a valid interval (avoid dividing by zero)
  if (_pulseInterval > 0) {
    // Frequency (Hz) = 1000 / interval_in_ms
    float freqHz = 1000.0 / (float)_pulseInterval;
    
    // Speed = Hz * Calibration Factor
    return freqHz * KPH_PER_HZ;
  }

  ErrorLogger::log(COMP_WINDSPEED, ERR_WIND_DIVIDE_ZERO, "_pulseInterval is 0 - returning 0.0");
  return 0.0;
}