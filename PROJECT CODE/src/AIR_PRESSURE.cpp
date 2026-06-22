#include <Arduino.h>
#include "AIR_PRESSURE.h"
#include "ERROR_LOGGER.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEA_LEVEL_HPA 1013.25

// Initialize the BME280 sensor
Adafruit_BME280 bme;



AirPressure::AirPressure() : pressure_(0.0), temperature_(0.0), humidity_(0.0), initialized_(false) {}

void AirPressure::sensor_setup() {
    Wire.setTimeOut(50); // Set 50ms timeout for I2C to prevent hangs
    
    if (!bme.begin(0x77, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor at 0x77, check wiring!");
        ErrorLogger::log(COMP_BME280, ERR_BME_NOT_FOUND, "I2C address 0x77 not responding");
        initialized_ = false;
    } else {
        Serial.println("BME280 sensor initialized successfully.");
        initialized_ = true;
    }
}

float AirPressure::readPressure() {
    if (!initialized_) return NAN;
    float raw = bme.readPressure();
    if (isnan(raw)) {
        ErrorLogger::log(COMP_BME280, ERR_BME_NAN_READ, "readPressure() returned NaN");
        return NAN;
    }
    pressure_ = raw / 100.0F;
    delay(50);
    return pressure_;
}

float AirPressure::readTemperature() {
    if (!initialized_) return NAN;
    temperature_ = bme.readTemperature();
    if (isnan(temperature_)) {
        ErrorLogger::log(COMP_BME280, ERR_BME_NAN_READ, "readTemperature() returned NaN");
        return NAN;
    }
    delay(50);
    return temperature_;
}

float AirPressure::readHumidity() {
    if (!initialized_) return NAN;
    humidity_ = bme.readHumidity();
    if (isnan(humidity_)) {
        ErrorLogger::log(COMP_BME280, ERR_BME_NAN_READ, "readHumidity() returned NaN");
        return NAN;
    }
    delay(50);
    return humidity_;
}

float AirPressure::readAltitude(float seaLevelhPa) {
    if (!initialized_) return NAN;
    altitude_ = bme.readAltitude(seaLevelhPa);
    if (isnan(altitude_)) {
        ErrorLogger::log(COMP_BME280, ERR_BME_NAN_READ, "readAltitude() returned NaN");
        return NAN;
    }
    delay(50);
    return altitude_;
}