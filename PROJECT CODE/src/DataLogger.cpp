#include "DataLogger.h"
#include <WiFi.h>
#include <HTTPClient.h>

// USE IP INSTEAD OF DOMAIN TO FIX ERROR 601
String THINGSPEAK_IP = "http://184.106.153.149"; 

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
    // Strip day name to avoid commas in the log (e.g. "Tuesday, 2026-07-14 13:50:41" → "2026-07-14T13:50:41")
    int commaIdx = timestamp.indexOf(", ");
    if (commaIdx != -1) timestamp = timestamp.substring(commaIdx + 2);
    timestamp.replace(" ", "T");

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
    if (archFile) {
        archFile.println(dataStr);
        archFile.close();
    }

    // 2. Write to Queue (Buffer)
    File queueFile = SD.open(_queueFileName, FILE_APPEND);
    if (queueFile) {
        queueFile.println(dataStr);
        queueFile.close();
        Serial.println("Queued: " + dataStr);
    }
}

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

        if (line == "") {
            popQueue(); // Remove empty lines
            continue;
        }

        Serial.println("[Queue] Attempting upload of oldest record...");

        String rawTime = getValueFromLog(line, "Time");
        bool validTs = rawTime.length() >= 19 && rawTime.substring(0, 4) != "2000";
        String created_at = validTs ? toISO8601(rawTime) : "";

        // CHANNEL 1
        String url1 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_1;
        if (validTs) url1 += "&created_at=" + created_at;
        url1 += "&field1=" + formatField(getValueFromLog(line, "Temp"));
        url1 += "&field2=" + formatField(getValueFromLog(line, "Hum"));
        url1 += "&field3=" + formatField(getValueFromLog(line, "Press"));
        url1 += "&field4=" + formatField(getValueFromLog(line, "Rain"));
        url1 += "&field5=" + formatField(getValueFromLog(line, "WSpd"));
        url1 += "&field6=" + formatField(getValueFromLog(line, "WDir"));
        url1 += "&field7=" + formatField(getValueFromLog(line, "Light"));
        url1 += "&field8=" + formatField(getValueFromLog(line, "SoilM"));

        bool success = gsmModule.sendThingSpeakRequest(url1);
        
        if (success) {
            delay(16000); // Rate limit

            // CHANNEL 2
            String url2 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_2;
            if (validTs) url2 += "&created_at=" + created_at;
            url2 += "&field1=" + getValueFromLog(line, "V33");
            url2 += "&field2=" + getValueFromLog(line, "V5");
            url2 += "&field3=" + getValueFromLog(line, "VBatt");
            url2 += "&field4=" + getValueFromLog(line, "VSol");
            url2 += "&field5=" + getValueFromLog(line, "VDC");
            gsmModule.sendThingSpeakRequest(url2);

            delay(16000); // Rate limit

            // CHANNEL 3
            String url3 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_3;
            if (validTs) url3 += "&created_at=" + created_at;
            url3 += "&field1=" + getValueFromLog(line, "CBatt");
            url3 += "&field2=" + getValueFromLog(line, "CSol");
            gsmModule.sendThingSpeakRequest(url3);

            Serial.println("[Queue] Upload successful. Popping from queue.");
            popQueue();
        } else {
            Serial.println("[Queue] Upload failed. GSM likely offline. Stopping.");
            break; // Stop trying if GSM is failing
        }
    }
}

bool DataLogger::uploadPendingDataWiFi(unsigned long startTimeMs, unsigned long tonLimitMs) {
    if (!SD.exists(_queueFileName)) {
        Serial.println("[Queue WiFi] No pending data.");
        return true;
    }

    HTTPClient http;
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

        if (line == "") {
            popQueue(); // Remove empty lines
            continue;
        }

        Serial.println("[Queue WiFi] Attempting upload of oldest record...");

        String rawTime = getValueFromLog(line, "Time");
        bool validTs = rawTime.length() >= 19 && rawTime.substring(0, 4) != "2000";
        String created_at = validTs ? toISO8601(rawTime) : "";

        // CHANNEL 1
        String url1 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_1;
        if (validTs) url1 += "&created_at=" + created_at;
        url1 += "&field1=" + formatField(getValueFromLog(line, "Temp"));
        url1 += "&field2=" + formatField(getValueFromLog(line, "Hum"));
        url1 += "&field3=" + formatField(getValueFromLog(line, "Press"));
        url1 += "&field4=" + formatField(getValueFromLog(line, "Rain"));
        url1 += "&field5=" + formatField(getValueFromLog(line, "WSpd"));
        url1 += "&field6=" + formatField(getValueFromLog(line, "WDir"));
        url1 += "&field7=" + formatField(getValueFromLog(line, "Light"));
        url1 += "&field8=" + formatField(getValueFromLog(line, "SoilM"));

        Serial.println("[Queue WiFi] Uploading Channel 1: " + url1);
        http.begin(url1);
        int httpCode = http.GET();
        String body = http.getString();
        Serial.printf("[Queue WiFi] CH1 HTTP %d, body: %s\n", httpCode, body.c_str());
        http.end();

        // Non-200 means network/WiFi problem — stop and retry next cycle
        if (httpCode != 200 && httpCode != 302) {
            Serial.printf("[Queue WiFi] Upload failed, HTTP code: %d. Stopping WiFi upload.\n", httpCode);
            anyFailure = true;
            break;
        }

        bool success = (body != "0");

        // Body "0" with created_at — ThingSpeak may have rejected the timestamp.
        // Retry once without created_at (let ThingSpeak use server time).
        if (!success && validTs) {
            Serial.println("[Queue WiFi] Body '0' with created_at. Retrying without timestamp...");
            String url1Retry = THINGSPEAK_IP + "/update?api_key=" + API_KEY_1;
            url1Retry += "&field1=" + formatField(getValueFromLog(line, "Temp"));
            url1Retry += "&field2=" + formatField(getValueFromLog(line, "Hum"));
            url1Retry += "&field3=" + formatField(getValueFromLog(line, "Press"));
            url1Retry += "&field4=" + formatField(getValueFromLog(line, "Rain"));
            url1Retry += "&field5=" + formatField(getValueFromLog(line, "WSpd"));
            url1Retry += "&field6=" + formatField(getValueFromLog(line, "WDir"));
            url1Retry += "&field7=" + formatField(getValueFromLog(line, "Light"));
            url1Retry += "&field8=" + formatField(getValueFromLog(line, "SoilM"));
            http.begin(url1Retry);
            httpCode = http.GET();
            body = http.getString();
            Serial.printf("[Queue WiFi] CH1 Retry HTTP %d, body: %s\n", httpCode, body.c_str());
            http.end();
            success = (httpCode == 200 || httpCode == 302) && body != "0";
            if (success) validTs = false; // Channels 2 & 3 should also skip created_at
        }

        // Still body "0" — record is permanently bad. Log it and skip.
        if (!success) {
            Serial.println("[Queue WiFi] Record rejected by ThingSpeak. Logging to /rejected.txt and skipping.");
            logRejected(line);
            popQueue();
            continue;
        }

        delay(16000); // Rate limit

        // CHANNEL 2
        String url2 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_2;
        if (validTs) url2 += "&created_at=" + created_at;
        url2 += "&field1=" + getValueFromLog(line, "V33");
        url2 += "&field2=" + getValueFromLog(line, "V5");
        url2 += "&field3=" + getValueFromLog(line, "VBatt");
        url2 += "&field4=" + getValueFromLog(line, "VSol");
        url2 += "&field5=" + getValueFromLog(line, "VDC");

        Serial.println("[Queue WiFi] Uploading Channel 2: " + url2);
        http.begin(url2);
        http.GET();
        http.end();

        delay(16000); // Rate limit

        // CHANNEL 3
        String url3 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_3;
        if (validTs) url3 += "&created_at=" + created_at;
        url3 += "&field1=" + getValueFromLog(line, "CBatt");
        url3 += "&field2=" + getValueFromLog(line, "CSol");

        Serial.println("[Queue WiFi] Uploading Channel 3: " + url3);
        http.begin(url3);
        http.GET();
        http.end();

        Serial.println("[Queue WiFi] Upload successful. Popping from queue.");
        popQueue();
    }

    return !anyFailure;
}

void DataLogger::logGSMStats(String timestamp, uint32_t sent, uint32_t received, uint32_t cycles) {
    File statsFile = SD.open("/gsm_usage.csv", FILE_APPEND);
    if (statsFile) {
        // Create header if file is new
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

bool DataLogger::popQueue() {
    if (!SD.exists(_queueFileName)) return false;

    File queueFile = SD.open(_queueFileName, FILE_READ);
    if (!queueFile) return false;

    File tempFile = SD.open("/temp_q.txt", FILE_WRITE);
    if (!tempFile) {
        queueFile.close();
        return false;
    }

    // Skip the first line
    bool skipped = false;
    while (queueFile.available()) {
        String line = queueFile.readStringUntil('\n');
        if (!skipped) {
            skipped = true;
            continue;
        }
        tempFile.println(line);
    }

    queueFile.close();
    tempFile.close();

    SD.remove(_queueFileName);
    SD.rename("/temp_q.txt", _queueFileName);
    return true;
}

String DataLogger::toISO8601(String timestamp) {
    // Timestamp in log is EAT (UTC+3): "2026-07-08T11:18:15"
    // ThingSpeak created_at requires UTC. Subtract 3 hours and append Z.
    if (timestamp.length() < 19) return timestamp;

    int year   = timestamp.substring(0, 4).toInt();
    int month  = timestamp.substring(5, 7).toInt();
    int day    = timestamp.substring(8, 10).toInt();
    int hour   = timestamp.substring(11, 13).toInt();
    int minute = timestamp.substring(14, 16).toInt();
    int second = timestamp.substring(17, 19).toInt();

    hour -= 3; // EAT → UTC
    if (hour < 0) {
        hour += 24;
        day -= 1;
        if (day < 1) {
            month -= 1;
            if (month < 1) { month = 12; year -= 1; }
            const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            day = (month == 2 && leap) ? 29 : daysInMonth[month - 1];
        }
    }

    char buf[22];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             year, month, day, hour, minute, second);
    return String(buf);
}

String DataLogger::formatField(String value) {
    // ThingSpeak rejects entries with created_at when all fields are "nan"
    // Return empty string so ThingSpeak stores null instead
    if (value == "nan" || value == "NaN") return "";
    return value;
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

void DataLogger::logRejected(String logLine) {
    File f = SD.open("/rejected.txt", FILE_APPEND);
    if (f) {
        f.println(logLine);
        f.close();
        Serial.println("[SD] Rejected record saved to /rejected.txt");
    } else {
        Serial.println("[SD] Failed to open /rejected.txt for writing");
    }
}
