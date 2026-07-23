#include "DataLogger.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// USE IP INSTEAD OF DOMAIN TO FIX ERROR 601
String THINGSPEAK_IP = "http://184.106.153.149"; 
// NOTE: ThingSpeak upload no longer used in uploadPendingData() below,
// left here in case you want to re-enable dual-upload later.

#ifndef STATION_CODE
#define STATION_CODE "AWS-UG-001"  // confirm/replace with your actual station code
#endif

DataLogger::DataLogger(int csPin) {
    _csPin = csPin;
    _fileName = "/datalog.txt";
    _queueFileName = "/queue.txt";
    _lastDataString = "";
}

void DataLogger::begin() {
    if (!SD.begin(_csPin)) {
        Serial.println("SD Card Mount Failed");
        return;
    }
    Serial.println("SD Card Initialized");
}

void DataLogger::logSensorData(String timestamp, SensorData data) {
    String dataStr = "";
    dataStr += "Time:" + timestamp + ",";
    dataStr += "Press:" + String(data.airPressure, 2) + ",";
    dataStr += "Alt:" + String(data.altitude, 2) + ",";
    dataStr += "Temp:" + String(data.temperature, 2) + ",";
    dataStr += "Hum:" + String(data.humidity, 2) + ",";
    dataStr += "Light:" + String(data.lightLevel, 2) + ",";
    dataStr += "SoilM:" + String(data.soilMoisture, 2) + ",";
    dataStr += "Rain:" + String(data.rainCount) + ",";
    dataStr += "WSpd:" + String(data.windSpeed, 2) + ",";
    dataStr += "WDir:" + String(data.windDirection) + ",";
    dataStr += "V33:" + String(data.volt_3v3, 2) + ",";
    dataStr += "V5:" + String(data.volt_5v, 2) + ",";
    dataStr += "VBatt:" + String(data.volt_batt, 2) + ",";
    dataStr += "VSol:" + String(data.volt_solar, 2) + ",";
    dataStr += "VDC:" + String(data.volt_dc, 2) + ",";
    dataStr += "CBatt:" + String(data.curr_batt, 2) + ",";
    dataStr += "CSol:" + String(data.curr_solar, 2);

    // 1. Write to Archive (Permanent)
    File archFile = SD.open(_fileName, FILE_APPEND);
    if (archFile) { archFile.println(dataStr); archFile.close(); }

    File queueFile = SD.open(_queueFileName, FILE_APPEND);
    if (queueFile) {
        queueFile.println(dataStr);
        queueFile.close();
        Serial.println("Queued: " + dataStr);
    }
}

// ── GSM (fallback) upload path ──
void DataLogger::uploadPendingData(GSM &gsmModule, unsigned long startTimeMs, unsigned long tonLimitMs) {
    while (true) {
        // Check if we still have time in the TON window (leave 45s margin for a full 3-channel upload)
        if (millis() - startTimeMs > (tonLimitMs - 45000)) {
            Serial.println("[Queue] TON limit approaching. Saving remaining for next cycle.");
            break;
        }

        if (!SD.exists(_queueFileName)) {
            Serial.println("[Queue] No pending data.");
            break;
        }

        File queueFile = SD.open(_queueFileName, FILE_READ);
        if (!queueFile || queueFile.size() == 0) {
            if (queueFile) queueFile.close();
            SD.remove(_queueFileName);
            break;
        }

        // Read the first line (oldest)
        String line = queueFile.readStringUntil('\n');
        line.trim();
        queueFile.close();

        if (line == "") { popQueue(); continue; }

        Serial.println("[Queue] Attempting upload of oldest record...");

        String isoTimestamp = getTimestampFromLog(line);

        // CHANNEL 1 - Weather
        String json1 = "{";
        json1 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
        json1 += "\"timestamp\":\"" + isoTimestamp + "\",";
        json1 += jsonField("pressure", getValueFromLog(line, "Press")) + ",";
        json1 += jsonField("altitude", getValueFromLog(line, "Alt")) + ",";
        json1 += jsonField("temperature", getValueFromLog(line, "Temp")) + ",";
        json1 += jsonField("humidity", getValueFromLog(line, "Hum")) + ",";
        json1 += "\"light\":" + getValueFromLog(line, "Light") + ",";
        json1 += "\"soil_moisture\":" + getValueFromLog(line, "SoilM") + ",";
        json1 += "\"rain\":" + getValueFromLog(line, "Rain") + ",";
        json1 += "\"wind_speed\":" + getValueFromLog(line, "WSpd") + ",";
        json1 += "\"wind_direction\":" + getValueFromLog(line, "WDir");
        json1 += "}";

        bool success = gsmModule.postToDjango(DJANGO_WEATHER_URL, json1);

        if (success) {
            delay(2000); // short pause for GSM stability

            // CHANNEL 2 - Voltage
            String json2 = "{";
            json2 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
            json2 += "\"timestamp\":\"" + isoTimestamp + "\",";
            json2 += "\"volt_3v3\":" + getValueFromLog(line, "V33") + ",";
            json2 += "\"volt_5v\":" + getValueFromLog(line, "V5") + ",";
            json2 += "\"volt_batt\":" + getValueFromLog(line, "VBatt") + ",";
            json2 += "\"volt_solar\":" + getValueFromLog(line, "VSol") + ",";
            json2 += "\"volt_dc\":" + getValueFromLog(line, "VDC");
            json2 += "}";
            gsmModule.postToDjango(DJANGO_VOLTAGE_URL, json2);

            delay(2000);

            // CHANNEL 3 - Current
            String json3 = "{";
            json3 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
            json3 += "\"timestamp\":\"" + isoTimestamp + "\",";
            json3 += "\"curr_batt\":" + getValueFromLog(line, "CBatt") + ",";
            json3 += "\"curr_solar\":" + getValueFromLog(line, "CSol");
            json3 += "}";
            gsmModule.postToDjango(DJANGO_CURRENT_URL, json3);

            Serial.println("[Queue] Upload successful. Popping from queue.");
            popQueue();
        } else {
            Serial.println("[Queue] Upload failed. GSM likely offline. Stopping.");
            break; // Stop trying if GSM is failing
        }
    }
}

// ── WiFi (primary) upload path ──
bool DataLogger::uploadPendingDataWiFi(unsigned long startTimeMs, unsigned long tonLimitMs) {
    if (!SD.exists(_queueFileName)) {
        Serial.println("[Queue WiFi] No pending data.");
        return true;
    }

    HTTPClient httpClient;
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    bool anyFailure = false;

    while (true) {
        // Check if we still have time in the TON window (leave 45s margin for a full 3-channel upload)
        if (millis() - startTimeMs > (tonLimitMs - 45000)) {
            Serial.println("[Queue WiFi] TON limit approaching. Saving remaining for next cycle.");
            break;
        }

        if (!SD.exists(_queueFileName)) {
            Serial.println("[Queue WiFi] No pending data.");
            break;
        }

        File queueFile = SD.open(_queueFileName, FILE_READ);
        if (!queueFile || queueFile.size() == 0) {
            if (queueFile) queueFile.close();
            SD.remove(_queueFileName);
            break;
        }

        // Read the first line (oldest)
        String line = queueFile.readStringUntil('\n');
        line.trim();
        queueFile.close();

        if (line == "") { popQueue(); continue; }

        Serial.println("[Queue WiFi] Attempting upload of oldest record...");
        String isoTimestamp = getTimestampFromLog(line);

        // CHANNEL 1 - Weather
        String json1 = "{";
        json1 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
        json1 += "\"timestamp\":\"" + isoTimestamp + "\",";
        json1 += jsonField("pressure", getValueFromLog(line, "Press")) + ",";
        json1 += jsonField("altitude", getValueFromLog(line, "Alt")) + ",";
        json1 += jsonField("temperature", getValueFromLog(line, "Temp")) + ",";
        json1 += jsonField("humidity", getValueFromLog(line, "Hum")) + ",";
        json1 += "\"light\":" + getValueFromLog(line, "Light") + ",";
        json1 += "\"soil_moisture\":" + getValueFromLog(line, "SoilM") + ",";
        json1 += "\"rain\":" + getValueFromLog(line, "Rain") + ",";
        json1 += "\"wind_speed\":" + getValueFromLog(line, "WSpd") + ",";
        json1 += "\"wind_direction\":" + getValueFromLog(line, "WDir");
        json1 += "}";

        httpClient.begin(secureClient, DJANGO_WEATHER_URL);
        httpClient.addHeader("Content-Type", "application/json");
        int code1 = httpClient.POST(json1);
        httpClient.end();
        Serial.printf("[Queue WiFi] Weather POST code: %d\n", code1);
        bool success = (code1 == 200 || code1 == 201);

        if (success) {
            // CHANNEL 2 - Voltage
            String json2 = "{";
            json2 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
            json2 += "\"timestamp\":\"" + isoTimestamp + "\",";
            json2 += "\"volt_3v3\":" + getValueFromLog(line, "V33") + ",";
            json2 += "\"volt_5v\":" + getValueFromLog(line, "V5") + ",";
            json2 += "\"volt_batt\":" + getValueFromLog(line, "VBatt") + ",";
            json2 += "\"volt_solar\":" + getValueFromLog(line, "VSol") + ",";
            json2 += "\"volt_dc\":" + getValueFromLog(line, "VDC");
            json2 += "}";

            httpClient.begin(secureClient, DJANGO_VOLTAGE_URL);
            httpClient.addHeader("Content-Type", "application/json");
            int code2 = httpClient.POST(json2);
            httpClient.end();
            Serial.printf("[Queue WiFi] Voltage POST code: %d\n", code2);

            // CHANNEL 3 - Current
            String json3 = "{";
            json3 += "\"station_id\":\"" + String(STATION_CODE) + "\",";
            json3 += "\"timestamp\":\"" + isoTimestamp + "\",";
            json3 += "\"curr_batt\":" + getValueFromLog(line, "CBatt") + ",";
            json3 += "\"curr_solar\":" + getValueFromLog(line, "CSol");
            json3 += "}";

            httpClient.begin(secureClient, DJANGO_CURRENT_URL);
            httpClient.addHeader("Content-Type", "application/json");
            int code3 = httpClient.POST(json3);
            httpClient.end();
            Serial.printf("[Queue WiFi] Current POST code: %d\n", code3);

            Serial.println("[Queue WiFi] Upload successful. Popping from queue.");
            popQueue();
        } else {
            Serial.printf("[Queue WiFi] Weather upload failed (code %d). Stopping WiFi upload.\n", code1);
            anyFailure = true;
            break;
        }
    }
    return !anyFailure;
}

void DataLogger::logGSMStats(String timestamp, uint32_t sent, uint32_t received, uint32_t cycles) {
    File statsFile = SD.open("/gsm_usage.csv", FILE_APPEND);
    if (statsFile) {
        if (statsFile.size() == 0) {
            statsFile.println("Timestamp,BytesSent,BytesReceived,TotalCycles,AvgKBPerCycle");
        }
        
        uint32_t total = sent + received;
        float avgKB = 0;
        if (cycles > 0) {
            avgKB = (total / 1024.0) / cycles;
        }

        statsFile.print(timestamp);
        statsFile.print(",");
        statsFile.print(sent);
        statsFile.print(",");
        statsFile.print(received);
        statsFile.print(",");
        statsFile.print(cycles);
        statsFile.print(",");
        statsFile.println(avgKB, 3);
        
        statsFile.close();
        Serial.println("[SD] GSM usage stats saved to /gsm_usage.csv");
    } else {
        Serial.println("[SD] Failed to open /gsm_usage.csv for writing");
    }
}

void DataLogger::logUSSDMessage(String timestamp, String message) {
    File f = SD.open("/ussd_log.txt", FILE_APPEND);
    if (f) {
        f.println(timestamp + "," + message);
        f.close();
        Serial.println("[SD] USSD message logged to /ussd_log.txt");
    } else {
        Serial.println("[SD] Failed to open /ussd_log.txt for writing");
    }
}

bool DataLogger::popQueue() {
    if (!SD.exists(_queueFileName)) return false;
    File queueFile = SD.open(_queueFileName, FILE_READ);
    if (!queueFile) return false;
    File tempFile = SD.open("/temp_q.txt", FILE_WRITE);
    if (!tempFile) { queueFile.close(); return false; }
    bool skipped = false;
    while (queueFile.available()) {
        String line = queueFile.readStringUntil('\n');
        if (!skipped) { skipped = true; continue; }
        tempFile.println(line);
    }
    queueFile.close();
    tempFile.close();
    SD.remove(_queueFileName);
    SD.rename("/temp_q.txt", _queueFileName);
    return true;
}

String DataLogger::getValueFromLog(String logLine, String label) {
    String searchKey = label + ":";
    int startIndex = logLine.indexOf(searchKey);
    if (startIndex == -1) return "0"; 
    
    startIndex += searchKey.length();
    int endIndex = logLine.indexOf(",", startIndex);
    if (endIndex == -1) endIndex = logLine.length(); 
    
    return logLine.substring(startIndex, endIndex);
}

String DataLogger::getTimestampFromLog(String logLine) {
    int startIndex = logLine.indexOf("Time:");
    if (startIndex == -1) return "";
    startIndex += 5; // length of "Time:"

    int firstComma = logLine.indexOf(",", startIndex);
    int secondComma = logLine.indexOf(",", firstComma + 1);
    if (firstComma == -1 || secondComma == -1) return "";

    String ts = logLine.substring(firstComma + 1, secondComma);
    ts.trim(); // "2026-07-02 14:11:12"

    // Convert to ISO 8601 so Django's parse_datetime() accepts it: "2026-07-02T14:11:12"
    ts.replace(" ", "T");
    return ts;
}

String DataLogger::jsonField(String key, String value) {
    if (value == "nan") {
        return "\"" + key + "\":\"nan\"";
    }
    return "\"" + key + "\":" + value;
}
