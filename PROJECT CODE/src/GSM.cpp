#include "GSM.h"

// ESP32 WROVER: Use 26/27 or 4/13. DO NOT use 16/17 if using PSRAM.
#define RX_GSM 16
#define TX_GSM 17

#define GSM_BAUD 115200
#define GPRS_APN "internet"

HardwareSerial SerialG = Serial2;

GSM::GSM() : networkLossStart(0) {}

void GSM::setupGSM() {
    SerialG.begin(GSM_BAUD, SERIAL_8N1, RX_GSM, TX_GSM);

    Serial.println("Waiting for SIM800C hardware auto-boot...");
    delay(5000);

    Serial.println("Initializing GSM at 115200...");

    bool gsmReady = false;
    int attempts = 0;
    while (!gsmReady && attempts < 10) {
        if (sendCommand("AT", 1500, true, "OK")) {
            gsmReady = true;
            Serial.println("GSM Ready.");
            break;
        }
        attempts++;
        Serial.println("Waiting for GSM response...");
        delay(1000);
    }

    if (!gsmReady) {
        Serial.println("GSM Failure (No AT response at 115200). Ensure SIM800C is awake and RX/TX pins are correct.");
        return;
    }

    // Check signal quality before starting GPRS
    sendCommand("AT+CSQ", 1500, true, "OK");

    connectGPRS();
}

void GSM::connectGPRS() {
    Serial.println("Configuring GPRS...");

    sendCommand("AT+HTTPTERM", 1000, true, "OK");
    sendCommand("AT+SAPBR=0,1", 1000, true, "OK");

    sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000, true, "OK");
    sendCommand(String("AT+SAPBR=3,1,\"APN\",\"") + GPRS_APN + "\"", 1000, true, "OK");
    sendCommand("AT+SAPBR=1,1", 5000, true, "OK");

    sendCommand("AT+SAPBR=2,1", 3000, true, "OK");
    sendCommand("AT+HTTPINIT", 1000, true, "OK");
    sendCommand("AT+HTTPPARA=\"CID\",1", 1000, true, "OK");
}

void GSM::sendThingSpeakRequest(String url) {
    sendCommand("AT+HTTPTERM", 1000, false, "OK");
    sendCommand("AT+HTTPINIT", 1000, false, "OK");

    Serial.println("Uploading: " + url);
    String cmd = String("AT+HTTPPARA=\"URL\",\"") + url + "\"";
    sendCommand(cmd, 2000, true, "OK");

    if (sendCommand("AT+HTTPACTION=0", 15000, true, "+HTTPACTION:")) {
        sendCommand("AT+HTTPREAD", 5000, true, "OK");
    } else {
        Serial.println("HTTPACTION failed.");
    }

    sendCommand("AT+HTTPTERM", 1000, false, "OK");
}

void GSM::checkNetworkHealth() {
    bool registered = verifyNetworkRegistration();
    if (registered) {
        if (networkLossStart != 0) {
            Serial.println("Network restored.");
        }
        networkLossStart = 0;
        return;
    }

    if (networkLossStart == 0) {
        networkLossStart = millis();
        Serial.println("Network not registered. Starting hold timer.");
    }

    unsigned long badDuration = millis() - networkLossStart;
    if (badDuration >= 300000) {
        Serial.println("Network lost >5 minutes; attempting recovery without power pin...");
        delay(8000);
        if (verifyGSMCommunication()) {
            connectGPRS();
            networkLossStart = 0;
        } else {
            Serial.println("SIM800C still unresponsive after reboot.");
        }
    } else {
        Serial.print("Network still bad for ");
        Serial.print(badDuration / 1000);
        Serial.println(" seconds.");
    }
}

bool GSM::sendCommand(const String& command, int timeout, bool debug, const String& expectedResponse) {
    while (SerialG.available()) {
        SerialG.read();
    }
    SerialG.println(command);

    if (debug) {
        Serial.print("Command: ");
        Serial.println(command);
    }

    String response = readResponse(timeout, debug);

    if (debug) {
        Serial.print("Response: ");
        Serial.println(response);
    }

    if (expectedResponse.length() == 0) {
        return true;
    }
    return response.indexOf(expectedResponse) != -1;
}

String GSM::readResponse(int timeout, bool debug) {
    String response;
    unsigned long deadline = millis() + timeout;

    while (millis() < deadline) {
        while (SerialG.available()) {
            char c = SerialG.read();
            response += c;
            if (debug) {
                Serial.write(c);
            }
        }
        delay(10);
    }
    return response;
}


bool GSM::verifyGSMCommunication() {
    return sendCommand("AT", 1500, true, "OK");
}

bool GSM::verifyNetworkRegistration() {
    while (SerialG.available()) {
        SerialG.read();
    }
    SerialG.println("AT+CREG?");
    delay(200);

    String response = readResponse(1500, true);
    return (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1);
}