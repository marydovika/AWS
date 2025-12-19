#include <Arduino.h>
#include "WIFI_CONNECTION.h"
#include "DHT22.h"
#include "AIR_PRESSURE.h"
#include "LIGHT_SENSOR.h"
#include "SOILMOISTURE.h"
#include "RTC.h"

WIFI_CONNECTION wifi_connection("SAM25", "Sam@2025");
DHTSensor dhtsensor;
AirPressure airpressure;
LightSensor lightsensor;
Soilmoisture soilmoisture;
Rtc rtc1;

// Uganda/East Africa Time (EAT) Configuration
const char* ntpServer = "africa.pool.ntp.org";
const long  gmtOffset_sec = 10800;      // UTC +3 hours
const int   daylightOffset_sec = 0;    // No DST in Uganda


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  wifi_connection.connect();
    if(wifi_connection.isConnected()) {
    Serial.println(String("Connected to WiFi SSID: ") + String(wifi_connection.getSSID().c_str()));
  } else {
    Serial.println("Not connected to WiFi.");
  }
 
  lightsensor.setupSensor();
  soilmoisture.setupSensor();
  rtc1.setupRTC();
   if(rtc1.syncWithNTP(ntpServer, gmtOffset_sec, daylightOffset_sec)) {
        Serial.println("Time synced for Uganda successfully.");
    } else {
        Serial.println("Time sync failed.");
    }
}

void loop() {
  
  // put your main code here, to run repeatedly:
  Serial.print("Light Level voltage: ");
  Serial.println(lightsensor.readLightLevel());
  Serial.print("V");

  Serial.print("Soil Moisture voltage: ");
  Serial.println(soilmoisture.readSoilMoisture());
  Serial.print("V");

  Serial.print("Soil Moisture digital value: ");
  Serial.println(soilmoisture.digitizeSoilMoisture());

  rtc1.printDateTime();
  delay(1000);
}


