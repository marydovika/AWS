#include <Arduino.h>
#include "AIR_PRESSURE.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define seaLevelhPa 1013.25

// Initialize the BME280 sensor
Adafruit_BME280 bme;



AirPressure::AirPressure() : pressure_(0.0), temperature_(0.0), humidity_(0.0) {}

void AirPressure::sensor_setup() {
    //Wire.begin(21, 22);  // SDA, SCL
    Serial.begin(9600);
    
    if (!bme.begin(0x77, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        //while (1);
    } else  if (!bme.begin(0x76, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        //while (1);
    } 
    
}

float AirPressure::readPressure() {
    pressure_ = bme.readPressure() / 100.0F; // Convert Pa to hPa
    delay(100); // brief delay to ensure sensor stability
    return pressure_;
}

float AirPressure::readTemperature() {
    temperature_ = bme.readTemperature();
    delay(100); // brief delay to ensure sensor stability
    return temperature_;
}

float AirPressure::readHumidity() {
    humidity_ = bme.readHumidity();
    delay(100); // brief delay to ensure sensor stability
    return humidity_;
}

float AirPressure::readAltitude(float eaLevelhPa) {
    altitude_ = bme.readAltitude(seaLevelhPa);
    delay(100); // brief delay to ensure sensor stability
    return bme.readAltitude(seaLevelhPa);
}