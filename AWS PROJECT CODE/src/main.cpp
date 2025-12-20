#include <Arduino.h>
//#include "WIFI_CONNECTION.h"
//#include "DHT22.h"
//#include "AIR_PRESSURE.h"
//#include "LIGHT_SENSOR.h"
//#include "SOILMOISTURE.h"
//#include "RTC.h"
//#include "MEMORY.h"
//#include "SIM.h"
#include <string>
using namespace std;


/*WIFI_CONNECTION wifi_connection("SAM25", "Sam@2025");
DHTSensor dhtsensor;
AirPressure airpressure;
LightSensor lightsensor;
Soilmoisture soilmoisture;
Rtc rtc1;
Memory SDcard;
//Sim simmodule;*/


/*// Uganda/East Africa Time (EAT) Configuration
const char* ntpServer = "africa.pool.ntp.org";
const long  gmtOffset_sec = 10800;      // UTC +3 hours
const int   daylightOffset_sec = 0;    // No DST in Uganda*/

/*// Test Server
const char server[]   = "httpbin.org";
const int  port       = 80;
string postData = "{\"id\":\"ESP32\", \"status\":\"active\"}";

string file_name = "/time data.txt";
*/
// ======================= NETWORK CONFIG =======================

/////////////////////////////////////////////////////////////////////////////////////
// ================= PINS =================
#define PWR_PIN     14
#define PIN_RX      16  // ESP32 RX -> SIM7000 TX
#define PIN_TX      17  // ESP32 TX -> SIM7000 RX
#define UART_BAUD   9600

// ================= APN =================
// Change "internet" to your provider's APN
//const char* apn = "internet"; 

#define SerialAT Serial2

void sendCommand(String command, int timeout) {
  Serial.print("Sending: ");
  Serial.println(command);
  
  // Clear buffer
  while(SerialAT.available()) SerialAT.read();
  
  // Send Command
  SerialAT.println(command);
  
  // Wait for response
  long int time = millis();
  while( (time + timeout) > millis()) {
    while(SerialAT.available()) {
      char c = SerialAT.read();
      Serial.write(c);
    }
  }
  Serial.println("\n-----------------------");
}
///////////////////////////////////////////////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  //Serial.begin(9600);
  /* wifi_connection.connect();
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
   SDcard.readData(file_name);*/
 
///////////////////////////////////////////////////////////////////////////////////
// 1. Init Console
  Serial.begin(9600);
  delay(100);

  // 2. Init Modem Serial
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(1000);

  // 3. Power On Sequence
  Serial.println("\n--- Powering ON Modem ---");
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH); delay(100);
  digitalWrite(PWR_PIN, LOW);  delay(1200); // Press key for 1.2s
  digitalWrite(PWR_PIN, HIGH);
  
  // Wait for boot (SIM7000 needs ~5-10 seconds to find network)
  Serial.println("Waiting 10s for boot and network search...");
  delay(10000);

  // 4. --- TEST SEQUENCE ---

  // TEST 1: Basic Functionality
  // Expect: OK
  sendCommand("AT", 2000);

  // TEST 2: Check SIM Card Status
  // Expect: +CPIN: READY
  sendCommand("AT+CPIN?", 2000);

  // TEST 3: Check Signal Quality
  // Expect: +CSQ: <rssi>,<ber> (RSSI should be higher than 5, ideally 15-30)
  sendCommand("AT+CSQ", 2000);

  // TEST 4: Set APN
  // This tells the modem which APN to use for context 1
  String apnCmd = "AT+CGDCONT=1,\"IP\",\"internet\"";
  sendCommand(apnCmd, 1000);

  // TEST 5: Check ISP Connection
  // Expect: +COPS: 0,0,"YourCarrierName",7
  sendCommand("AT+COPS?", 1000);

  // TEST 6: Check Data Attachment
  // Expect: +CGATT: 1 (1 means connected to internet, 0 means disconnected)
  sendCommand("AT+CGATT?", 3000);

  // TEST 7: Ping Google (Ultimate Test)
  // This performs a real ping to 8.8.8.8
  Serial.println("Attempting Ping to 8.8.8.8...");
  sendCommand("AT+CQPING=1,\"8.8.8.8\"", 8000);

  Serial.println("\n--- End of Auto Test. You can now type AT commands manually below ---");

//////////////////////////////////////////////////////////////////////////////////
}



void loop() {
  //float lightVal = lightsensor.readLightLevel();
  //simmodule.sendData(server, postData);
  //rtc1.printDateTime();
  //String lightStrArduino = String(lightVal, 2); // 2 decimal places
  //string lightStr = string(lightStrArduino.c_str());
  //string dataframe1 = lightStr + "," + rtc1.getDateTime();
  //SDcard.writeData(file_name, dataframe1);

  /////////////////////////////////////////////////////////////////////////
   // Passthrough: Type in Serial Monitor -> Send to Modem
  if (Serial.available()) {
    SerialAT.write(Serial.read());
  }
  // Modem Reply -> Send to Serial Monitor
  if (SerialAT.available()) {
    Serial.write(SerialAT.read());
  }
  //////////////////////////////////////////////////////////////////////////
 
  delay(1000);
}


