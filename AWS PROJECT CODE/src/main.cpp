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


// Uganda/East Africa Time (EAT) Configuration
const char* ntpServer = "africa.pool.ntp.org";
const long  gmtOffset_sec = 10800;      // UTC +3 hours
const int   daylightOffset_sec = 0;    // No DST in Uganda*/


string file_name = "/time data.txt";

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

    SDcard.setupMemory();
  //SDcard.clearFile(file_name);
  SDcard.createFile(file_name);
   SDcard.readData(file_name);
 
  simmodule.setupGSM();
  loramodule.setupLora();

  dhtsensor.getsensor();
  airpressure.sensor_setup();
}



void loop() {
  float lightVal = lightsensor.readLightLevel();
  Serial.print("Light Voltage: ");
  Serial.print(lightVal);
  rtc1.printDateTime();
  simmodule.sendData("AT", 2000); // Check signal quality
  loramodule.sendData("AT+CDEVEUI?", 2000); // Check signal quality
  soilmoisture.readSoilMoisture();
  Serial.print("Soil Moisture Voltage: ");
  Serial.println(soilmoisture.readSoilMoisture());
  dhtsensor.readHumidity();
  dhtsensor.readTemperature();
  airpressure.readPressure();
  airpressure.readAltitude(1013.25);

  SDcard.readData(file_name);
  


  //String lightStrArduino = String(lightVal, 2); // 2 decimal places
  //string lightStr = string(lightStrArduino.c_str());
  //string dataframe1 = lightStr + "," + rtc1.getDateTime();
  //SDcard.writeData(file_name, dataframe1);

  //geaddaadwa

  delay(1000);
}


