#include "DAVIS.h"

// =========================================================
// GLOBAL HELPERS FOR INTERRUPTS
// =========================================================

// 1. Global pointer to the class instance
Davis* davisInstance = nullptr;

// 2. Global Interrupt Service Routine (ISR) wrapper
void IRAM_ATTR rainISR() {
  if (davisInstance) {
    davisInstance->handleInterrupt();
  }
}

// =========================================================
// CLASS IMPLEMENTATION
// =========================================================

Davis::Davis(){
  rainCounter = 0;
  lastResetTime = 0;
  lastDebounceTime = 0;
  // Point the global pointer to this object so the ISR can find it
  davisInstance = this;
}

void Davis::setupRainGauge(){
  // Use INPUT_PULLUP if your sensor connects to Ground when tipped.
  // Note: Some ESP32 pins (like 34, 35, 36, 39) are INPUT ONLY and do not have internal pullups.
  // If using pin 34, make sure you have a physical 10k resistor from Pin 34 to 3.3V.
  // If using a standard GPIO (like 25, 26, 27), INPUT_PULLUP works fine.
  pinMode(rainfallpin, INPUT_PULLUP); 
  
  // Initialize timer
  lastResetTime = millis();

  // Attach the Interrupt
  // FALLING = triggers when switch connects to Ground
  attachInterrupt(digitalPinToInterrupt(rainfallpin), rainISR, FALLING);
}   

// This runs automatically when the bucket tips
void IRAM_ATTR Davis::handleInterrupt() {
  unsigned long currentTime = millis();

  // Debounce logic
  if ((currentTime - lastDebounceTime) > DEBOUNCE_TIME) {
    rainCounter++;
    lastDebounceTime = currentTime;
  }
}

int Davis::readRainGauge(){
  unsigned long currentTime = millis();

  // --- HOURLY RESET LOGIC ---
  if (currentTime - lastResetTime >= RESET_INTERVAL) {
    
    // Optional: Pause interrupts briefly while resetting to be perfectly safe
    noInterrupts(); 
    rainCounter = 0;
    lastResetTime = currentTime;
    interrupts();
    
  }

  return rainCounter;
}