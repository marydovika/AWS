#include "DataLogger.h"
#include "ERROR_LOGGER.h"

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
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_MOUNT_FAIL, "SD.begin() failed");
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
    if (archFile) {
        archFile.println(dataStr);
        archFile.close();
    } else {
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_ARCHIVE_WRITE_FAIL, "datalog.txt open failed - reading lost");
    }

    // 2. Write to Queue (Buffer)
    File queueFile = SD.open(_queueFileName, FILE_APPEND);
    if (queueFile) {
        queueFile.println(dataStr);
        queueFile.close();
        Serial.println("Queued: " + dataStr);
    } else {
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_QUEUE_WRITE_FAIL, "queue.txt open failed - reading not queued");
    }
}

void DataLogger::uploadPendingData(GSM &gsmModule, unsigned long startTimeMs, unsigned long tonLimitMs) {
    while (true) {
        if (millis() - startTimeMs > (tonLimitMs - 45000)) {
            Serial.println("[Queue] TON limit approaching. Saving remaining for next cycle.");
            break;
        }

        if (!SD.exists(_queueFileName)) {
            Serial.println("[Queue] No pending data.");
            break;
        }

        File queueFile = SD.open(_queueFileName, FILE_READ);
        if (!queueFile) {
            ErrorLogger::log(COMP_SD_CARD, ERR_SD_QUEUE_READ_FAIL, "queue.txt exists but open failed");
            break;
        }
        if (queueFile.size() == 0) {
            queueFile.close();
            SD.remove(_queueFileName);
            break;
        }

        String line = queueFile.readStringUntil('\n');
        line.trim();
        queueFile.close();

        if (line == "") {
            popQueue();
            continue;
        }

        Serial.println("[Queue] Attempting upload of oldest record...");
        
        String url1 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_1;
        url1 += "&field1=" + getValueFromLog(line, "Temp");
        url1 += "&field2=" + getValueFromLog(line, "Hum");
        url1 += "&field3=" + getValueFromLog(line, "Press");
        url1 += "&field4=" + getValueFromLog(line, "Rain");
        url1 += "&field5=" + getValueFromLog(line, "WSpd");
        url1 += "&field6=" + getValueFromLog(line, "WDir");
        url1 += "&field7=" + getValueFromLog(line, "Light");
        url1 += "&field8=" + getValueFromLog(line, "SoilM");

        bool success = gsmModule.sendThingSpeakRequest(url1);
        
        if (success) {
            delay(16000);
            
            String url2 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_2;
            url2 += "&field1=" + getValueFromLog(line, "V33");
            url2 += "&field2=" + getValueFromLog(line, "V5");
            url2 += "&field3=" + getValueFromLog(line, "VBatt");
            url2 += "&field4=" + getValueFromLog(line, "VSol");
            url2 += "&field5=" + getValueFromLog(line, "VDC");
            gsmModule.sendThingSpeakRequest(url2); // failure logged inside GSM.cpp if it occurs
            
            delay(16000);

            String url3 = THINGSPEAK_IP + "/update?api_key=" + API_KEY_3;
            url3 += "&field1=" + getValueFromLog(line, "CBatt");
            url3 += "&field2=" + getValueFromLog(line, "CSol");
            gsmModule.sendThingSpeakRequest(url3); // failure logged inside GSM.cpp if it occurs

            Serial.println("[Queue] Upload successful. Popping from queue.");
            popQueue();
        } else {
            // Root cause already logged inside GSM::sendThingSpeakRequest()
            Serial.println("[Queue] Upload failed. GSM likely offline. Stopping.");
            break;
        }
    }
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
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_STATS_WRITE_FAIL, "gsm_usage.csv open failed");
    }
}

bool DataLogger::popQueue() {
    if (!SD.exists(_queueFileName)) return false;

    File queueFile = SD.open(_queueFileName, FILE_READ);
    if (!queueFile) {
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_QUEUE_READ_FAIL, "popQueue: queue.txt open failed");
        return false;
    }

    File tempFile = SD.open("/temp_q.txt", FILE_WRITE);
    if (!tempFile) {
        queueFile.close();
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_TEMP_FILE_FAIL, "temp_q.txt open failed during popQueue");
        return false;
    }

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

String DataLogger::getValueFromLog(String logLine, String label) {
    String searchKey = label + ":";
    int startIndex = logLine.indexOf(searchKey);
    if (startIndex == -1) {
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_PARSE_FAIL, ("Label not found: " + label).c_str());
        return "0";
    }
    
    startIndex += searchKey.length();
    int endIndex = logLine.indexOf(",", startIndex);
    if (endIndex == -1) endIndex = logLine.length(); 
    
    return logLine.substring(startIndex, endIndex);
}