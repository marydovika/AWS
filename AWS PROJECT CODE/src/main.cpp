#include <Arduino.h>
#include "WIFI_CONNECTION.h"
#include "DHT22.h"
#include "AIR_PRESSURE.h"
#include "LIGHT_SENSOR.h"
#include "SOILMOISTURE.h"
#include "RTC.h"
#include "MEMORY.h"
#include "GSM.h"
#include "LORA.h"
#include "DAVIS.h"
#include "WINDSPEED.h"
#include "WIND_DIRECTION.h"
#include "POWER_MONITORING.h"
#include <string>
using namespace std;


WIFI_CONNECTION wifi_connection("transysit-2G", "V@m@Li@1");
DHTSensor dhtsensor;
AirPressure airpressure;
LightSensor lightsensor;
Soilmoisture soilmoisture;
Rtc rtc1;
Memory SDcard;
GSM simmodule;
Lora loramodule;
Davis davisrain;

WindSpeedSensor windspeedsensor;
WindDirectionSensor winddirectionsensor;
PowerMonitoring powermonitoring;

// Uganda/East Africa Time (EAT) Configuration
const char* ntpServer = "africa.pool.ntp.org";
const long  gmtOffset_sec = 10800;      // UTC +3 hours
const int   daylightOffset_sec = 0;    // No DST in Uganda*/


string file_name = "/time data.txt";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
/*
   wifi_connection.connect();
    if(wifi_connection.isConnected()) {
    Serial.println(String("Connected to WiFi SSID: ") + String(wifi_connection.getSSID().c_str()));
  } else {
    Serial.println("Not connected to WiFi.");
  }
 
  lightsensor.setupSensor();
  soilmoisture.setupSensor();
  
    SDcard.setupMemory();
  //SDcard.clearFile(file_name);
  SDcard.createFile(file_name);
   SDcard.readData(file_name);
 
  simmodule.setupGSM();
  loramodule.setupLora();

  dhtsensor.getsensor();
  */

 rtc1.setupRTC();
   if(rtc1.syncWithNTP(ntpServer, gmtOffset_sec, daylightOffset_sec)) {
        Serial.println("Time synced for Uganda successfully.");
    } else {
        Serial.println("Time sync failed.");
    }

  airpressure.sensor_setup();
  
  davisrain.setupRainGauge();
  windspeedsensor.setupSensor();
  winddirectionsensor.setupSensor();
  powermonitoring.begin(21, 22); // ESP32 default I2C pins
}



void loop() {
  /*
  float lightVal = lightsensor.readLightLevel();
  Serial.print("Light Voltage: ");
  Serial.print(lightVal);
  
  simmodule.sendData("AT", 2000); // Check signal quality
  loramodule.sendData("AT+CDEVEUI?", 2000); // Check signal quality
  soilmoisture.readSoilMoisture();
  Serial.print("Soil Moisture Voltage: ");
  Serial.println(soilmoisture.readSoilMoisture());
  dhtsensor.readHumidity();
  dhtsensor.readTemperature();
  

  SDcard.readData(file_name);
  */

  rtc1.printDateTime();
  airpressure.readPressure();
  airpressure.readAltitude(1013.25);

  Serial.print("Air Pressure (hPa): ");
  Serial.println(airpressure.readPressure());
  Serial.print("Altitude (m): ");
  Serial.println(airpressure.readAltitude(1013.25));

   // Read data using the class
  if (powermonitoring.readData()) {
    
    // Retrieve the data struct
    VoltageData voltages = powermonitoring.getData();

    Serial.println("OK");
    Serial.println("-------------------------");
    Serial.printf("3.3V Rail: %.2f V\n", voltages.v1);
    Serial.printf("5V Rail:   %.2f V\n", voltages.v2);
    Serial.printf("Battery:   %.2f V\n", voltages.v3);
    Serial.printf("Solar:     %.2f V\n", voltages.v4);
    Serial.printf("DC In:     %.2f V\n", voltages.v5);
    Serial.println("-------------------------");

  } else {
    Serial.println("Connection Failed.");
  }
  

  //String lightStrArduino = String(lightVal, 2); // 2 decimal places
  //string lightStr = string(lightStrArduino.c_str());
  //string dataframe1 = lightStr + "," + rtc1.getDateTime();
  //SDcard.writeData(file_name, dataframe1);

  
  int rainCount = davisrain.readRainGauge();
  Serial.print("Rainfall Count (last hour): "); 
  Serial.println(rainCount);
  
  float windSpeedKPH = windspeedsensor.readWindSpeedKPH();
  Serial.print("Wind Speed (km/h): ");  
  Serial.println(windSpeedKPH);
  
 
  int windDirectionDeg = winddirectionsensor.readWindDirectionDeg();
  Serial.print("Wind Direction (Degrees): "); 
  Serial.println(windDirectionDeg);
  
  

  delay(100);
}


