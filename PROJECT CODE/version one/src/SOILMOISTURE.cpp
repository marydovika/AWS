#include <Arduino.h>
#include "SOILMOISTURE.h"
#define ANALOG_PIN 34  // Example analog pin for soil moisture sensor

//=SOIL MOISTURE CAN ALSO BE CONSIDERED RAINFALL

Soilmoisture::Soilmoisture() : _pin(ANALOG_PIN) {}

float Soilmoisture::readSoilMoisture() {
    Serial.begin(9600);
    int adc = analogRead(_pin);
    moisture_voltage_ = 3.3 - ((adc/4095.0) * 3.25); // Convert ADC value to voltage
    delay(100); // brief delay to ensure sensor stability
    return moisture_voltage_;
}

int Soilmoisture::digitizeSoilMoisture() {
    float digital_value = readSoilMoisture();
    if (digital_value < 0.8) {
        digital_value_ = 0; // Dry
    } else if (digital_value >= 0.8 && digital_value < 2.5) {
        digital_value_ = 1; // Moist
    } 

    return digital_value_;
}

void Soilmoisture::setupSensor() {
    pinMode(_pin, INPUT);
}