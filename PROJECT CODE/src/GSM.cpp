#include "GSM.h"

// ESP32 WROVER: Use 26/27 or 4/13. DO NOT use 16/17 if using PSRAM.
// Assuming Standard ESP32 here based on your previous code.
#define RX_GSM 16 
#define TX_GSM 17

HardwareSerial SerialG = Serial2;

GSM::GSM() {}

void GSM::setupGSM() {
    SerialG.begin(9600, SERIAL_8N1, RX_GSM, TX_GSM); 
    delay(1000);
    Serial.println("Initializing GSM...");

    // Handshake
    bool gsmReady = false;
    int attempts = 0;
    while (!gsmReady && attempts < 10) {
        SerialG.println("AT"); 
        delay(500);
        if (SerialG.available()) {
            String response = SerialG.readString();
            if (response.indexOf("OK") != -1) {
                gsmReady = true;
                Serial.println("GSM Ready.");
            }
        }
        attempts++;
    }
    
    if(gsmReady) {
        connectGPRS();
    } else {
        Serial.println("GSM Failure.");
    }
}

void GSM::connectGPRS() {
    Serial.println("Connecting to GPRS...");
    sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000, true);
    // Replace 'internet' with your APN if different (e.g., "mtn", "airtel")
    sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 1000, true); 
    sendCommand("AT+SAPBR=1,1", 3000, true); // Enable GPRS
    sendCommand("AT+SAPBR=2,1", 3000, true); // Check IP
    sendCommand("AT+HTTPINIT", 1000, true);  // Init HTTP
}

void GSM::sendThingSpeakRequest(String url) {
    // 1. Set URL
    Serial.println("Uploading to ThingSpeak...");
    String cmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    sendCommand(cmd, 2000, false);

    // 2. GET Request (Action 0)
    sendCommand("AT+HTTPACTION=0", 5000, true);

    // 3. Read Response (Optional, just checking status)
    // You can add logic here to check for "200 OK"
    sendCommand("AT+HTTPREAD", 2000, true);
}

void GSM::sendCommand(const String& command, int timeout, boolean debug) {
    while(SerialG.available()) SerialG.read(); // Clear buffer
    SerialG.println(command);
    
    long int time = millis();
    while((time + timeout) > millis()) {
        while(SerialG.available()) {
            char c = SerialG.read();
            if(debug) Serial.write(c);
        }
    }
    if(debug) Serial.println();
}