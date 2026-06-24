#include "GSM.h"
#include <SD.h>

// Persistent counters across deep sleep
RTC_DATA_ATTR uint32_t gsmBytesSent = 0;
RTC_DATA_ATTR uint32_t gsmBytesReceived = 0;
RTC_DATA_ATTR uint32_t gsmCycles = 0;

// ESP32 WROVER: Use 26/27 or 4/13. DO NOT use 16/17 if using PSRAM.
#define RX_GSM 16 
#define TX_GSM 17

HardwareSerial SerialG = Serial2;

GSM::GSM() {}

void GSM::countSent(const String& s) {
    gsmBytesSent += s.length();
}

void GSM::countReceived(const String& s) {
    gsmBytesReceived += s.length();
}

uint32_t GSM::getTotalBytesSent() { return gsmBytesSent; }
uint32_t GSM::getTotalBytesReceived() { return gsmBytesReceived; }
uint32_t GSM::getCycleCount() { return gsmCycles; }
void GSM::resetByteCounters() { gsmBytesSent = 0; gsmBytesReceived = 0; gsmCycles = 0; }

void GSM::setupGSM() {
    gsmCycles++;
    SerialG.begin(9600, SERIAL_8N1, RX_GSM, TX_GSM); 
    delay(1000);
    Serial.println("Initializing GSM...");

    // Handshake (log responses for debugging)
    bool gsmReady = false;
    int attempts = 0;
    while (!gsmReady && attempts < 10) {
        String cmd = "AT";
        SerialG.println(cmd); 
        countSent(cmd + "\r\n");
        delay(500);
        if (SerialG.available()) {
            String response = SerialG.readString();
            countReceived(response);
            if (response.indexOf("OK") != -1) {
                gsmReady = true;
                Serial.println("GSM Ready.");
            }
        }
        attempts++;
    }
    
    if(gsmReady) {
        sendCommand("AT+CLTS=1", 500, false); // Enable network time sync
        if (waitForNetwork(40000)) { // Wait up to 40 seconds
            connectGPRS();
        } else {
            Serial.println("Network Registration Failed (Timeout).");
        }
    } else {
        Serial.println("GSM Failure (Check Wiring/Power).");
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
        delay(2000); // Wait 2s between checks
    }
    Serial.println(" Failed (Timeout)");
    return false;
}

String GSM::getNetworkTime() {
    // 1. Force the module to update its clock from the network
    sendCommand("AT+CLTS=1", 500, false);
    
    for (int retry = 0; retry < 3; retry++) {
        // Try GSMLOC (uses tower location to get very accurate time)
        // Format: +CIPGSMLOC: 0,2026/06/17,09:42:30
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
        
        // 2. Fallback to CCLK
        // Format: +CCLK: "yy/mm/dd,hh:mm:ss+zz"
        response = sendCommandWithResponse("AT+CCLK?", 2000, true);
        int firstQuote = response.indexOf('\"');
        int lastQuote = response.lastIndexOf('\"');
        if (firstQuote != -1 && lastQuote != -1) {
            String timeStr = response.substring(firstQuote + 1, lastQuote);
            if (!timeStr.startsWith("04")) { // Not the factory default
                 return timeStr;
            }
        }

        Serial.println("[GSM] Time not ready, waiting...");
        delay(2000);
    }
    return "";
}

void GSM::connectGPRS() {
    Serial.println("Configuring GPRS...");
    
    // 1. CLEAN UP START: Close previous connections to stop "ERROR"
    sendCommand("AT+HTTPTERM", 1000, true); 
    sendCommand("AT+SAPBR=0,1", 1000, true); 

    // 2. Start Connection
    sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 2000, true);
    sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 2000, true); 
    
    Serial.println("[GSM] Opening GPRS Bearer (can take 30s)...");
    sendCommand("AT+SAPBR=1,1", 30000, true); // Increase to 30s
    
    // 3. Verify IP
    String ipResp = sendCommandWithResponse("AT+SAPBR=2,1", 5000, true); 
    if (ipResp.indexOf("0.0.0.0") != -1 || ipResp.indexOf("ERROR") != -1) {
        Serial.println("[GSM] Bearer failed to get IP. Retrying SAPBR=1,1...");
        sendCommand("AT+SAPBR=1,1", 10000, true);
    }
    
    // 4. Initialize HTTP Service
    sendCommand("AT+HTTPINIT", 2000, true);  
}

bool GSM::sendThingSpeakRequest(String url) {
    // Terminate any stuck previous sessions just in case
    sendCommand("AT+HTTPTERM", 500, false);
    sendCommand("AT+HTTPINIT", 500, false);

    Serial.println("Uploading: " + url);
    
    // Set URL
    String cmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    sendCommand(cmd, 2000, false);

    // GET Request (Action 0)
    String actionCmd = "AT+HTTPACTION=0";
    SerialG.println(actionCmd);
    countSent(actionCmd + "\r\n");
    
    String actionResp = "";
    unsigned long start = millis();
    // Wait for BOTH "OK" (immediate) and then "+HTTPACTION:" (async)
    bool seenOK = false;
    bool seenAction = false;
    
    while (millis() - start < 45000) { // Increase timeout to 45s for slow GPRS
        while (SerialG.available()) {
            char c = SerialG.read();
            actionResp += c;
            countReceived(String(c));
            Serial.write(c);
        }
        
        if (actionResp.indexOf("OK") != -1) seenOK = true;
        
        // We need the code after HTTPACTION: 0,200,length
        // So we wait until we see a newline after the action string
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
    if (actionResp.indexOf(",200,") != -1) { // More specific check for 200 OK
        success = true;
        Serial.println("[GSM] Upload Success (200 OK)");
    } else {
        Serial.println("[GSM] Upload Failed or Timed Out");
    }

    // Read Response
    sendCommand("AT+HTTPREAD", 2000, true);
    
    // Close session
    sendCommand("AT+HTTPTERM", 500, false); 

    return success;
}

void GSM::sendCommand(const String& command, int timeout, boolean debug) {
    while(SerialG.available()) {
        char c = SerialG.read();
        countReceived(String(c));
    }
    SerialG.println(command);
    countSent(command + "\r\n");

    String resp = "";
    unsigned long start = millis();
    while (millis() - start < (unsigned long)timeout) {
        while (SerialG.available()) {
            char c = SerialG.read();
            resp += c;
            countReceived(String(c));
            if (debug) Serial.write(c);
        }
        // Early exit if we see common terminators
        if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) {
            break; 
        }
        delay(1); // Feed the watchdog
    }
    if (debug) Serial.println();
}

String GSM::sendCommandWithResponse(const String& command, int timeout, boolean debug) {
    while(SerialG.available()) {
        char c = SerialG.read();
        countReceived(String(c));
    }
    SerialG.println(command);
    countSent(command + "\r\n");

    String resp = "";
    unsigned long start = millis();
    while (millis() - start < (unsigned long)timeout) {
        while (SerialG.available()) {
            char c = SerialG.read();
            resp += c;
            countReceived(String(c));
            if (debug) Serial.write(c);
        }
        if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) {
            break; 
        }
        delay(1);
    }
    if (debug) Serial.println();
    return resp;
}

String GSM::queryUSSD(const String& ussdCode, int timeoutMs) {
    // 1. Clear any serial garbage
    while(SerialG.available()) {
        SerialG.read();
    }
    
    // Ensure USSD presentation is enabled (GSM/7-bit format)
    sendCommand("AT+CUSD=1", 1000, false);
    
    // Send the query command
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
            Serial.write(c); // Print to debug console
        }
        
        if (response.indexOf("+CUSD:") != -1) {
            seenCUSD = true;
        }
        
        // Wait for the full USSD output line (usually ends with a newline after the DCS field)
        if (seenCUSD && (response.endsWith("\n") || response.endsWith("\r"))) {
            delay(100);
            while (SerialG.available()) {
                char c = SerialG.read();
                response += c;
                countReceived(String(c));
            }
            break;
        }
        
        delay(1); // Feed ESP32 watchdog
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
    
    // Check if it's hex-encoded (UCS2)
    // UCS2 hex string length must be a multiple of 4, and only contain hex chars
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
        // Convert 4 hex chars to uint16_t code point
        String chunk = hexStr.substring(i, i + 4);
        uint32_t val = strtoul(chunk.c_str(), NULL, 16);
        
        // Convert code point to UTF-8
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
    // Convert to lowercase for case-insensitive matching
    String lower = msg;
    lower.toLowerCase();
    
    // Find unit indicators
    int unitIdx = -1;
    float multiplier = 1.0f; // Default unit: MB
    
    if ((unitIdx = lower.indexOf("gb")) != -1) {
        multiplier = 1024.0f;
    } else if ((unitIdx = lower.indexOf("mb")) != -1) {
        multiplier = 1.0f;
    } else if ((unitIdx = lower.indexOf("kb")) != -1) {
        multiplier = 1.0f / 1024.0f;
    } else if ((unitIdx = lower.indexOf("bytes")) != -1) {
        multiplier = 1.0f / (1024.0f * 1024.0f);
    } else {
        // No recognizable data units, return -1.0 to indicate parsing failure
        return -1.0f;
    }
    
    // Walk backward from the unit index to find the start of the number
    int startIdx = unitIdx - 1;
    // Skip spaces
    while (startIdx >= 0 && (lower[startIdx] == ' ' || lower[startIdx] == '\t' || lower[startIdx] == '\r' || lower[startIdx] == '\n')) {
        startIdx--;
    }
    
    // Now extract digits, decimal points, and commas
    int numEndIdx = startIdx;
    while (startIdx >= 0 && ((lower[startIdx] >= '0' && lower[startIdx] <= '9') || lower[startIdx] == '.' || lower[startIdx] == ',')) {
        startIdx--;
    }
    // startIdx is now one character before the number starts
    startIdx++; 
    
    if (startIdx > numEndIdx) return -1.0f; // No number found
    
    String numStr = lower.substring(startIdx, numEndIdx + 1);
    numStr.replace(",", ""); // Remove commas if any
    
    float val = numStr.toFloat();
    return val * multiplier;
}


