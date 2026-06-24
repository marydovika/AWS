#include <Arduino.h>
#include <Wire.h>
#include "WIFI_CONNECTION.h"
#include "DHT22.h"
#include "AIR_PRESSURE.h"
#include "LIGHT_SENSOR.h"
#include "SOILMOISTURE.h"
#include "RTC.h"
#include "GSM.h"
#include "DAVIS.h"
#include "WINDSPEED.h"
#include "WIND_DIRECTION.h"
#include "POWER_MONITORING.h"
#include "DataLogger.h"
#include "SensorData.h"

// Objects
DHTSensor dhtsensor;
AirPressure airpressure;
LightSensor lightsensor;
Soilmoisture soilmoisture;
Rtc rtc1;
Davis davisrain;
WindSpeedSensor windspeedsensor;
WindDirectionSensor winddirectionsensor;
PowerMonitoring powermonitoring;

GSM simmodule;
DataLogger dataLogger(4); // CS pin 4 for SD card

// Timer variables
unsigned long lastUploadTime = 0;
const long uploadInterval = 15000; // 15 seconds

void setup() {
    Serial.begin(115200); // ← changed from 9600 to match your GSM debug work

    // 1. Initialize RTC
    rtc1.setupRTC();

    // 2. Initialize Sensors
    dhtsensor.getsensor();
    airpressure.sensor_setup();
    davisrain.setupRainGauge();
    windspeedsensor.setupSensor();
    winddirectionsensor.setupSensor();
    lightsensor.setupSensor();
    soilmoisture.setupSensor();
    powermonitoring.begin(21, 22);

    // 3. Initialize SD and GSM
    dataLogger.begin();
    simmodule.setupGSM();

    Serial.println("Setup complete.");
}

void loop() {
    SensorData currentData;

    // ── 1. Air Pressure / BME280 ──────────────────────────
    float p = airpressure.readPressure();
    currentData.airPressure = isnan(p) ? 0.0 : p;

    float alt = airpressure.readAltitude(1013.25);
    currentData.altitude = isnan(alt) ? 0.0 : alt;

    float t = airpressure.readTemperature();
    currentData.temperature = isnan(t) ? 0.0 : t;

    float h = airpressure.readHumidity();
    currentData.humidity = isnan(h) ? 0.0 : h;

    // ── 2. Power Monitoring ───────────────────────────────
    if (powermonitoring.readData()) {
        VoltageData v = powermonitoring.getData();
        currentData.volt_3v3   = v.v1;
        currentData.volt_5v    = v.v2;
        currentData.volt_batt  = v.v3;
        currentData.volt_solar = v.v4;
        currentData.volt_dc    = v.v5;
        currentData.curr_batt  = v.v6;
        currentData.curr_solar = v.v7;
    } else {
        currentData.volt_3v3   = 0.0;
        currentData.volt_5v    = 0.0;
        currentData.volt_batt  = 0.0;
        currentData.volt_solar = 0.0;
        currentData.volt_dc    = 0.0;
        currentData.curr_batt  = 0.0;
        currentData.curr_solar = 0.0;
    }

    // ── 3. Other Sensors ──────────────────────────────────
    currentData.lightLevel     = lightsensor.readLightLevel();
    currentData.soilMoisture   = soilmoisture.readSoilMoisture();
    currentData.rainCount      = davisrain.readRainGauge();
    currentData.windSpeed      = windspeedsensor.readWindSpeedKPH();
    currentData.windDirection  = winddirectionsensor.readWindDirectionDeg();

    // ── 4. Timestamp & SD Logging ─────────────────────────
    String timeStr = String(rtc1.getDateTime().c_str());
    dataLogger.logSensorData(timeStr, currentData);

    // ── 5. Upload every 15 seconds ────────────────────────
    if (millis() - lastUploadTime >= uploadInterval) {
        lastUploadTime = millis();

        Serial.println("Triggering Upload Sequence...");

        String json = "{";
        json += "\"station_id\":\"AWS-UG-001\",";
        json += "\"raw\":\"";
        json += "Time:"  + timeStr                              + ",";
        json += "Press:" + String(currentData.airPressure,  2) + ",";
        json += "Alt:"   + String(currentData.altitude,     2) + ",";
        json += "Temp:"  + String(currentData.temperature,  2) + ",";
        json += "Hum:"   + String(currentData.humidity,     2) + ",";
        json += "Light:" + String(currentData.lightLevel,   2) + ",";
        json += "SoilM:" + String(currentData.soilMoisture, 2) + ",";
        json += "Rain:"  + String(currentData.rainCount)       + ",";
        json += "WSpd:"  + String(currentData.windSpeed,    2) + ",";
        json += "WDir:"  + String(currentData.windDirection)   + ",";
        json += "V33:"   + String(currentData.volt_3v3,     2) + ",";
        json += "V5:"    + String(currentData.volt_5v,      2) + ",";
        json += "VBatt:" + String(currentData.volt_batt,    2) + ",";
        json += "VSol:"  + String(currentData.volt_solar,   2) + ",";
        json += "VDC:"   + String(currentData.volt_dc,      2) + ",";
        json += "CBatt:" + String(currentData.curr_batt,    2) + ",";
        json += "CSol:"  + String(currentData.curr_solar,   2);
        json += "\"}";

        Serial.println("[GSM] Payload: " + json);
        simmodule.postToDjango(json);
    }

    delay(1000); // 1 second loop delay
}