#include "GSM.h"
#include <SD.h>
#include <Preferences.h>

// Persistent counters across deep sleep / power cuts
uint32_t gsmBytesSent = 0;
uint32_t gsmBytesReceived = 0;
uint32_t gsmCycles = 0;

// ESP32 WROVER: Use 26/27 or 4/13. DO NOT use 16/17 if using PSRAM.
#define RX_GSM 16
#define TX_GSM 17

#define GSM_BAUD 115200
#define GPRS_APN "internet"

HardwareSerial SerialG = Serial2;

GSM::GSM() : networkLossStart(0) {}

void GSM::countSent(const String &s) {
    gsmBytesSent += s.length();
}

void GSM::countReceived(const String &s) {
    gsmBytesReceived += s.length();
}

uint32_t GSM::getTotalBytesSent() { return gsmBytesSent; }
uint32_t GSM::getTotalBytesReceived() { return gsmBytesReceived; }
uint32_t GSM::getCycleCount() { return gsmCycles; }

void GSM::resetByteCounters() {
    gsmBytesSent = 0;
    gsmBytesReceived = 0;
    gsmCycles = 0;
    
    Preferences prefs;
    prefs.begin("gsm_stats", false);
    prefs.putUInt("sent", 0);
    prefs.putUInt("received", 0);
    prefs.putUInt("cycles", 0);
    prefs.end();
}

void GSM::initSerial() {
    SerialG.begin(GSM_BAUD, SERIAL_8N1, RX_GSM, TX_GSM);
    delay(1000);
}

void GSM::setupGSM() {
    Preferences prefs;
    prefs.begin("gsm_stats", false);
    gsmBytesSent = prefs.getUInt("sent", 0);
    gsmBytesReceived = prefs.getUInt("received", 0);
    gsmCycles = prefs.getUInt("cycles", 0);
    
    gsmCycles++;
    prefs.putUInt("cycles", gsmCycles);
    prefs.end();

    SerialG.begin(GSM_BAUD, SERIAL_8N1, RX_GSM, TX_GSM);

    Serial.println("Waiting for SIM800C/SIM800X hardware auto-boot...");
    delay(2000);

    // Clear serial buffer and reset SIM800 AT parser
    SerialG.println();
    delay(100);
    while (SerialG.available()) {
        SerialG.read();
    }

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
        Serial.println("GSM Failure (No AT response at 115200). Ensure SIM800C/SIM800X is awake and RX/TX pins are correct.");
        return;
    }

    // Check signal quality
    sendCommand("AT+CSQ", 1500, true, "OK");

    // Enable network time sync
    sendCommand("AT+CLTS=1", 500, false);

    // Wait for network registration (up to 40 seconds)
    if (waitForNetwork(40000)) {
        connectGPRS();
    } else {
        Serial.println("Network Registration Failed (Timeout).");
    }
}

bool GSM::waitForNetwork(int timeoutMs) {
    Serial.print("Waiting for network registration...");
    unsigned long start = millis();
    while (millis() - start < (unsigned long)timeoutMs) {
        String resp = sendCommandWithResponse("AT+CREG?", 1000, false);
        // Look for +CREG: 0,1 (Home) or 0,5 (Roaming)
        if (resp.indexOf(",1") != -1 || resp.indexOf(",5") != -1) {
            Serial.println(" Registered!");
            return true;
        }
        Serial.print(".");
        delay(2000);
    }
    Serial.println(" Failed (Timeout)");
    return false;
}

void GSM::connectGPRS() {
    Serial.println("Configuring GPRS...");
    
    // 1. Terminate any stuck HTTP session
    sendCommand("AT+HTTPTERM", 1500, true); 
    
    // 2. Close GPRS bearer cleanly.
    sendCommand("AT+SAPBR=0,1", 5000, true); 

    // 3. Configure the bearer profile
    sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 3000, true);
    sendCommand(String("AT+SAPBR=3,1,\"APN\",\"") + GPRS_APN + "\"", 3000, true); 
    
    // DNS config
    sendCommand("AT+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\"", 2000, true);  

    Serial.println("[GSM] Opening GPRS Bearer (can take 30s)...");
    sendCommand("AT+SAPBR=1,1", 30000, true); 
    
    // Wait for IP to be assigned
    bool gotIP = false;
    String ipResp = "";
    for (int i = 0; i < 10; i++) {
        ipResp = sendCommandWithResponse("AT+SAPBR=2,1", 3000, true);
        if (ipResp.indexOf("0.0.0.0") == -1 && ipResp.indexOf("1,1") != -1) {
            Serial.println("[GSM] Got IP!");
            gotIP = true;
            break;
        }
        Serial.println("[GSM] Waiting for IP...");
        delay(3000);
    }

    if (!gotIP) {
        Serial.println("[GSM] GPRS failed. Skipping HTTP init.");
        return; 
    }

    // DNS config after bearer confirmed up
    sendCommand("AT+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\"", 2000, true);  
    sendCommand("AT+HTTPINIT", 3000, true);  
}

void GSM::disconnectGPRS() {
    Serial.println("[GSM] Disconnecting GPRS and terminating HTTP before sleep...");
    sendCommand("AT+HTTPTERM", 1500, true); 
    sendCommand("AT+SAPBR=0,1", 5000, true); 

    Preferences prefs;
    prefs.begin("gsm_stats", false);
    prefs.putUInt("sent", gsmBytesSent);
    prefs.putUInt("received", gsmBytesReceived);
    prefs.end();
    Serial.println("[GSM] Stats saved to preferences.");
}

bool GSM::sendThingSpeakRequest(String url) {
    sendCommand("AT+HTTPTERM", 500, false);
    sendCommand("AT+HTTPINIT", 500, false);

    Serial.println("Uploading: " + url);
    
    String cmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    sendCommand(cmd, 2000, false);

    String actionCmd = "AT+HTTPACTION=0";
    SerialG.println(actionCmd);
    countSent(actionCmd + "\r\n");
    
    String actionResp = "";
    unsigned long start = millis();
    bool seenOK = false;
    bool seenAction = false;
    
    while (millis() - start < 45000) { 
        while (SerialG.available()) {
            char c = SerialG.read();
            actionResp += c;
            countReceived(String(c));
            Serial.write(c);
        }
        
        if (actionResp.indexOf("OK") != -1) seenOK = true;
        
        if (actionResp.indexOf("+HTTPACTION:") != -1) {
            if (actionResp.endsWith("\n") || actionResp.endsWith("\r")) {
                seenAction = true;
                break;
            }
        }
        delay(1); 
    }
    Serial.println();

    bool success = false;
    if (actionResp.indexOf(",200,") != -1) { 
        success = true;
        Serial.println("[GSM] Upload Success (200 OK)");
    } else {
        Serial.println("[GSM] Upload Failed or Timed Out");
    }

    sendCommand("AT+HTTPREAD", 2000, true);
    sendCommand("AT+HTTPTERM", 500, false); 

    return success;
}

bool GSM::postToDjango(String url, String jsonPayload) {
    Serial.println("[GSM] Posting to Django..." + url);

    String bearerCheck = sendCommandWithResponse("AT+SAPBR=2,1", 3000, true);
    if (bearerCheck.indexOf("0.0.0.0") != -1 || bearerCheck.indexOf("ERROR") != -1) {
        Serial.println("[GSM] Bearer down. Reconnecting...");
        connectGPRS();
        bearerCheck = sendCommandWithResponse("AT+SAPBR=2,1", 3000, true);
        if (bearerCheck.indexOf("0.0.0.0") != -1 || bearerCheck.indexOf("ERROR") != -1) {
            Serial.println("[GSM] Bearer still down. Aborting POST.");
            return false;
        }
    }

    sendCommand("AT+HTTPTERM", 1000, false);
    sendCommand("AT+HTTPINIT", 500, false);
    
    String urlCmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    sendCommand(urlCmd, 2000, false);

    if (url.startsWith("https://")) {
        sendCommand("AT+HTTPSSL=1", 1000, false);
    } else {
        sendCommand("AT+HTTPSSL=0", 1000, false);
    }

    sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000, false);

    int payloadLen = jsonPayload.length();
    String dataCmd = "AT+HTTPDATA=" + String(payloadLen) + ",60000";

    SerialG.println(dataCmd);
    countSent(dataCmd + "\r\n");

    unsigned long downloadStart = millis();
    String dataResp = "";
    bool readyToDownload = false;
    while (millis() - downloadStart < 5000) {
        while (SerialG.available()) {
            char c = SerialG.read();
            dataResp += c;
            countReceived(String(c));
            Serial.write(c);
        }
        if (dataResp.indexOf("DOWNLOAD") != -1) {
            readyToDownload = true;
            break;
        }
        delay(1);
    }
    Serial.println("[GSM] HTTPDATA resp: " + dataResp);
    if (!readyToDownload) {
        Serial.println("[GSM] Failed to receive DOWNLOAD prompt. Aborting.");
        sendCommand("AT+HTTPTERM", 500, false);
        return false;
    }

    SerialG.print(jsonPayload);
    countSent(jsonPayload);

    String okResp = "";
    unsigned long start = millis();
    bool payloadOk = false;
    while (millis() - start < 5000) {
        while (SerialG.available()) {
            char c = SerialG.read();
            okResp += c;
            countReceived(String(c));
        }
        if (okResp.indexOf("OK") != -1) {
            payloadOk = true;
            break;
        }
        delay(1);
    }
    if (!payloadOk) {
        Serial.println("[GSM] Failed to receive OK after payload. Aborting.");
        sendCommand("AT+HTTPTERM", 500, false);
        return false;
    }

    String actionResp = sendCommandWithResponse("AT+HTTPACTION=1", 15000, true);
    unsigned long actionStart = millis();
    while (millis() - actionStart < 15000) {
        while (SerialG.available()) {
            char c = SerialG.read();
            actionResp += c;
            countReceived(String(c));
            Serial.write(c);
        }
        if (actionResp.indexOf("+HTTPACTION:") != -1 &&
            (actionResp.endsWith("\n") || actionResp.endsWith("\r"))) {
            break;
        }
        delay(10);
    }

    bool success = (actionResp.indexOf(",200,") != -1 || actionResp.indexOf(",201,") != -1);
    if (success) {
        Serial.println("[GSM] POST successful!");
    } else {
        Serial.println("[GSM] POST failed: " + actionResp);
    }

    sendCommand("AT+HTTPREAD", 3000, true);
    sendCommand("AT+HTTPTERM", 1000, false);
    return success;
}

void GSM::postToDjango(String jsonPayload) {
    postToDjango(DJANGO_URL, jsonPayload);
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
        Serial.println("Network lost >5 minutes; attempting recovery...");
        delay(8000);
        if (verifyGSMCommunication()) {
            connectGPRS();
            networkLossStart = 0;
        } else {
            Serial.println("SIM800X still unresponsive.");
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
    countSent(command + "\r\n");

    if (debug) {
        Serial.print("Command: ");
        Serial.println(command);
    }

    String response = readResponse(timeout, debug, expectedResponse);

    if (debug) {
        Serial.print("Response: ");
        Serial.println(response);
    }

    if (expectedResponse.length() == 0) {
        return true;
    }
    return response.indexOf(expectedResponse) != -1;
}

void GSM::sendCommand(const String& command, int timeout, bool debug) {
    sendCommand(command, timeout, debug, "");
}

String GSM::sendCommandWithResponse(const String& command, int timeout, bool debug) {
    while (SerialG.available()) {
        SerialG.read();
    }
    SerialG.println(command);
    countSent(command + "\r\n");

    if (debug) {
        Serial.print("Command: ");
        Serial.println(command);
    }

    String response = readResponse(timeout, debug);

    if (debug) {
        Serial.print("Response: ");
        Serial.println(response);
    }
    return response;
}

String GSM::readResponse(int timeout, bool debug, const String& expectedResponse) {
    String response;
    unsigned long deadline = millis() + timeout;

    while (millis() < deadline) {
        while (SerialG.available()) {
            char c = SerialG.read();
            response += c;
            countReceived(String(c));
            if (debug) {
                Serial.write(c);
            }
        }
        
        if (expectedResponse.length() > 0 && response.indexOf(expectedResponse) != -1) {
            break;
        }
        
        if (response.indexOf("OK\r\n") != -1 || response.indexOf("ERROR\r\n") != -1 || response.indexOf("OK\n") != -1 || response.indexOf("ERROR\n") != -1) {
            break;
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

String GSM::getNetworkTime() {
    sendCommand("AT+CLTS=1", 500, false);
    
    for (int retry = 0; retry < 3; retry++) {
        String response = sendCommandWithResponse("AT+CIPGSMLOC=2,1", 10000, true);
        
        if (response.indexOf("601") != -1) {
            Serial.println("[GSM] Bearer error, skipping GSMLOC.");
        } else {
            int firstComma = response.indexOf(',');
            int secondComma = response.indexOf(',', firstComma + 1);
            if (firstComma != -1 && secondComma != -1) {
                String fullDate = response.substring(secondComma + 1);
                fullDate.trim();
                if (fullDate.startsWith("20")) {
                    return fullDate.substring(2); // "26/06/17,09:42:30"
                }
            }
        }
        
        response = sendCommandWithResponse("AT+CCLK?", 2000, true);
        int firstQuote = response.indexOf('\"');
        int lastQuote = response.lastIndexOf('\"');
        if (firstQuote != -1 && lastQuote != -1) {
            String timeStr = response.substring(firstQuote + 1, lastQuote);
            if (!timeStr.startsWith("04")) { // Not factory default
                 return timeStr;
            }
        }

        Serial.println("[GSM] Time not ready, waiting...");
        delay(2000);
    }
    return "";
}

String GSM::queryUSSD(const String& ussdCode, int timeoutMs) {
    while (SerialG.available()) {
        SerialG.read();
    }
    
    sendCommand("AT+CUSD=1", 1000, false);
    
    String cmd = "AT+CUSD=1,\"" + ussdCode + "\"";
    SerialG.println(cmd);
    countSent(cmd + "\r\n");
    
    String response = "";
    unsigned long start = millis();
    bool seenCUSD = false;
    
    while (millis() - start < (unsigned long)timeoutMs) {
        while (SerialG.available()) {
            char c = SerialG.read();
            response += c;
            countReceived(String(c));
            Serial.write(c);
        }
        
        if (response.indexOf("+CUSD:") != -1) {
            seenCUSD = true;
        }
        
        if (seenCUSD && (response.endsWith("\n") || response.endsWith("\r"))) {
            delay(100);
            while (SerialG.available()) {
                char c = SerialG.read();
                response += c;
                countReceived(String(c));
            }
            break;
        }
        delay(1);
    }
    return response;
}

String GSM::extractUSSDMessage(const String& cusdResponse) {
    int cusdIdx = cusdResponse.indexOf("+CUSD:");
    if (cusdIdx == -1) return "";
    
    int firstQuote = cusdResponse.indexOf('\"', cusdIdx);
    if (firstQuote == -1) return "";
    
    int secondQuote = cusdResponse.indexOf('\"', firstQuote + 1);
    if (secondQuote == -1) return "";
    
    String rawMsg = cusdResponse.substring(firstQuote + 1, secondQuote);
    
    bool isHex = (rawMsg.length() > 0) && (rawMsg.length() % 4 == 0);
    if (isHex) {
        for (unsigned int i = 0; i < rawMsg.length(); i++) {
            char c = rawMsg[i];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                isHex = false;
                break;
            }
        }
    }
    
    if (isHex) {
        return decodeUCS2(rawMsg);
    }
    return rawMsg;
}

String GSM::decodeUCS2(const String& hexStr) {
    String decoded = "";
    for (unsigned int i = 0; i + 3 < hexStr.length(); i += 4) {
        String chunk = hexStr.substring(i, i + 4);
        uint32_t val = strtoul(chunk.c_str(), NULL, 16);
        
        if (val <= 0x7F) {
            decoded += (char)val;
        } else if (val <= 0x7FF) {
            decoded += (char)(0xC0 | (val >> 6));
            decoded += (char)(0x80 | (val & 0x3F));
        } else {
            decoded += (char)(0xE0 | (val >> 12));
            decoded += (char)(0x80 | ((val >> 6) & 0x3F));
            decoded += (char)(0x80 | (val & 0x3F));
        }
    }
    return decoded;
}

float GSM::parseBalanceFromUSSD(const String& msg) {
    String lower = msg;
    lower.toLowerCase();
    
    int unitIdx = -1;
    float multiplier = 1.0f;
    
    if ((unitIdx = lower.indexOf("gb")) != -1) {
        multiplier = 1024.0f;
    } else if ((unitIdx = lower.indexOf("mb")) != -1) {
        multiplier = 1.0f;
    } else if ((unitIdx = lower.indexOf("kb")) != -1) {
        multiplier = 1.0f / 1024.0f;
    } else if ((unitIdx = lower.indexOf("bytes")) != -1) {
        multiplier = 1.0f / (1024.0f * 1024.0f);
    } else {
        return -1.0f;
    }
    
    int startIdx = unitIdx - 1;
    while (startIdx >= 0 && (lower[startIdx] == ' ' || lower[startIdx] == '\t' || lower[startIdx] == '\r' || lower[startIdx] == '\n')) {
        startIdx--;
    }
    
    int numEndIdx = startIdx;
    while (startIdx >= 0 && ((lower[startIdx] >= '0' && lower[startIdx] <= '9') || lower[startIdx] == '.' || lower[startIdx] == ',')) {
        startIdx--;
    }
    startIdx++;
    
    if (startIdx > numEndIdx) return -1.0f;
    
    String numStr = lower.substring(startIdx, numEndIdx + 1);
    numStr.replace(",", "");
    
    float val = numStr.toFloat();
    return val * multiplier;
}