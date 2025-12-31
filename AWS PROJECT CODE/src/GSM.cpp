#include "GSM.h"

// Check your board: If using ESP32 WROVER, do NOT use 16/17. 
// Use 26/27 or 4/13 instead.
#define RX_GSM 16 
#define TX_GSM 17

HardwareSerial SerialG = Serial2;

GSM::GSM() {}

void GSM::setupGSM() {
    // 1. Begin Serial
    // Note: Ensure ESP32 RX connects to GSM TX, and ESP32 TX connects to GSM RX
    SerialG.begin(9600, SERIAL_8N1, RX_GSM, TX_GSM); 
    delay(1000); // Give port time to open

    Serial.println("Initializing GSM...");

    // 2. THE HANDSHAKE LOOP
    // We send "AT" repeatedly until we get an "OK".
    // This handles Auto-Bauding and ensures the module is powered and ready.
    bool gsmReady = false;
    int attempts = 0;
    
    while (!gsmReady && attempts < 10) {
        Serial.print("Sending Handshake (AT)... ");
        
        // Clear buffer before sending
        while(SerialG.available()) SerialG.read();
        
        SerialG.println("AT"); // Send AT
        
        // Listen for response for 1 second
        long start = millis();
        while (millis() - start < 1000) {
            if (SerialG.available()) {
                String response = SerialG.readString();
                Serial.print("Response: ");
                Serial.println(response);
                
                if (response.indexOf("OK") != -1) {
                    gsmReady = true;
                    Serial.println("GSM Module Paired via UART!");
                    break;
                }
            }
        }
        
        if (!gsmReady) {
            Serial.println("No response. Retrying...");
        }
        attempts++;
    }

    if (!gsmReady) {
        Serial.println("ERROR: GSM Module not responding. Check Wiring/Power.");
    }
}

void GSM::sendData(const String& command, int timeout) {
    Serial.print("Sending: ");
    Serial.println(command);

    while(SerialG.available()) SerialG.read(); // Flush input
    SerialG.println(command);

    long int time = millis();
    while((time + timeout) > millis()) {
        while(SerialG.available()) {
            char c = SerialG.read();
            Serial.write(c);
        }
    }
    Serial.println("\n-----------------------");
}