#include <Arduino.h>
#include "WIFI_CONNECTION.h"
#include "DHT22.h"
#include "AIR_PRESSURE.h"

WIFI_CONNECTION wifi_connection("SAM25", "Sam@2025");
DHTSensor dhtsensor;
AirPressure airpressure;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  wifi_connection.connect();
    if(wifi_connection.isConnected()) {
    Serial.println("Connected to WiFi SSID: " + String(wifi_connection.getSSID().c_str()));
  } else {
    Serial.println("Not connected to WiFi.");
  }
 
  airpressure.sensor_setup();
}

void loop() {
  
  // put your main code here, to run repeatedly:
  Serial.print("Temperature: ");
  Serial.print(airpressure.readTemperature());
  Serial.print("Pressure: ");
  Serial.print(airpressure.readPressure());
  Serial.print("Humidity: ");
  Serial.println(airpressure.readHumidity());
  Serial.println("altitude: ");
  Serial.println(airpressure.readAltitude(1013.25));
  delay(2000);
}


