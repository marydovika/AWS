#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Buffer for large HTTP responses

#include "SIM.h"
#include <Arduino.h>
#include <string>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
using namespace std;

#define PWR_PIN     14

// ESP32 Serial1 Pins (UART1)
#define PIN_RX      16  // Connect to SIM7000 TX
#define PIN_TX      17  // Connect to SIM7000 RX

// Modem Baud Rate (Default is usually 9600 or 115200)
#define UART_BAUD   9600 


// ======================= NETWORK CONFIG =======================
const char apn[]      = "internet"; // CHANGE THIS to your APN
const char gprsUser[] = "";
const char gprsPass[] = "";

// Test Server
const char server[]   = "httpbin.org";
const int  port       = 80;

// ======================= OBJECTS =======================
// We use 'Serial1' for the modem connection
#define SerialAT  Serial1   

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

void gsmpulsekey();

Sim::Sim() : connected_(false), apn_("internet"), data_(""), server_url_path_("") {}

void Sim::setupSIM() {
     // 1. Initialize Computer Serial
  Serial.begin(9600);
  delay(10);

  // 2. Initialize Modem Serial (UART1)
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);

  // 3. Power On Sequence (GPIO 14)
  Serial.println("\nStarting Up...");
    gsmpulsekey();
  
  Serial.println("Waiting for modem to boot...");
  delay(6000); 

  // 4. Initialize Modem
  Serial.println("Initializing firmware...");
  if (!modem.restart()) {
    Serial.println("Modem not responding. Check wiring/baud rate.");
  } else {
    String modemInfo = modem.getModemInfo();
    Serial.print("Modem Info: ");
    Serial.println(modemInfo);
  }

  
}

bool Sim::connectNetwork(const string& apn, const string& gprsUser, const string& gprsPass){
    apn_ = apn;
    gprsUser_ = gprsUser;
    gprsPass_ = gprsPass;
    // 5. DISABLE GPS (GSM Only)
  Serial.println("Disabling GPS to save power...");
  modem.disableGPS(); 

  // 6. Network Registration
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail. (Check Antenna & Power)");
    //delay(10000);
    return false;
  }
  Serial.println(" success");

  // 7. GPRS Connection
  Serial.print("Connecting to APN (");
  Serial.print(apn_.c_str());
  Serial.print(")...");
  if (!modem.gprsConnect(apn_.c_str(), gprsUser_.c_str(), gprsPass_.c_str())) {
    Serial.println(" fail");
    //delay(10000);
    return false;
  }
  Serial.println(" success");
    connected_ = true;
    return connected_;
}

bool Sim::isConnected() {
    if(!modem.isGprsConnected()) {
        connected_ = false;
    } else {
        connected_ = true;
    }
    return connected_;
}

void Sim::sendData(const string& server_url_path, const string& data) {
    if(!isConnected()) {
        Serial.println("Not connected to network. Cannot send data.");
         // Optional: Try to reconnect here automatically?
        if(!connectNetwork(apn_, gprsUser_, gprsPass_)) {
            Serial.println("Reconnect failed.");
            return;
        }
    }

    Serial.println("Preparing HTTP POST request...");

    //manually building HTTP POST request
    http.beginRequest();
    http.post(server_url_path.c_str());

    //set headers
    // for json data use "application/json"
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", data.length());

    //send body
    http.beginBody();
    http.print(data.c_str());
    http.endRequest();

    //get response
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("HTTP Status Code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);

    http.stop();
}

void gsmpulsekey() {
    Serial.println("Powering on modem via GPIO 14...");
    pinMode(PWR_PIN, OUTPUT);
    // Pulse PWRKEY Low for >1s to turn on
    digitalWrite(PWR_PIN, HIGH); 
    delay(100);
    digitalWrite(PWR_PIN, LOW);
    delay(1200); 
    digitalWrite(PWR_PIN, HIGH);
 }