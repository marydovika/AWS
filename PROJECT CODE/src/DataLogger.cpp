#include "DataLogger.h"
#include "ERROR_LOGGER.h"

// USE IP INSTEAD OF DOMAIN TO FIX ERROR 601
String THINGSPEAK_IP = "http://184.106.153.149";

static const int MAX_RETRIES = 3;
static const int MAX_LOG_LINES = 500;
static const String TEMP_FILE = "/datalog_tmp.txt";
static const unsigned long TS_INTERVAL = 16000UL;

DataLogger::DataLogger(int csPin) {
    _csPin = csPin;
    _fileName = "/datalog.txt";
    _lastDataString = "";
    _sdAvailable = false;
    _uploadState = UPLOAD_IDLE;
    _lastUploadTime = 0;
    _uploadPending = false;
}

void DataLogger::begin() {
    if (!SD.begin(_csPin)) {
        Serial.println("SD Card Mount Failed");
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_MOUNT_FAIL, "SD.begin() failed");
        return;
    }
    Serial.println("[DataLogger] SD Card Initialized");
    _sdAvailable = true;
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
    dataStr += "VDC:" + String(data.volt_dc, 2);

    File file = SD.open(_fileName, FILE_APPEND);
    if (file) {
        file.println(dataStr);
        file.close();
        Serial.println("Logged: " + dataStr);
        _lastDataString = dataStr; 
    } else {
        Serial.println("Error writing to SD");
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_WRITE_FAIL, "SD.open() in APPEND mode failed - data lost");
    }
}

String DataLogger::getValueFromLog(String logLine, String label) {
    String searchKey = label + ":";
    int startIndex = logLine.indexOf(searchKey);
    if (startIndex == -1){
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_PARSE_FAIL, ("Label not found: " + label).c_str());
        return "0"; 
    }
    
    startIndex += searchKey.length();
    int endIndex = logLine.indexOf(",", startIndex);
    if (endIndex == -1) endIndex = logLine.length();
    
    return logLine.substring(startIndex, endIndex);
}

String DataLogger::_buildUrl(String apiKey,
    std::initializer_list<std::pair<String, String>> fields) {
    String url = THINGSPEAK_IP + "/update?api_key=" + apiKey;
    for (auto &f : fields) {
        url += "&" + f.second + "=" + getValueFromLog(_lastDataString, f.first);
    }
    return url;
}

bool DataLogger::_sendWithRetry(GSM &gsmModule, String url) {
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        int responseCode = gsmModule.sendThingSpeakRequest(url);

        // ThingSpeak success returns positive entry_id.
        if (responseCode > 0) {
            Serial.println("[DataLogger] Upload success, entry_id=" + String(responseCode));
            return true;
        }

        Serial.println("[DataLogger] Upload failed (code=" + String(responseCode) +
                       "), retry " + String(attempt) + "/" + String(MAX_RETRIES));
        delay(2000);
    }
    return false;
}

String DataLogger::_peekFirstPendingLine() {
    if (!_sdAvailable) return "";
    File file = SD.open(_fileName, FILE_READ);
    if (!file) return "";

    String line = "";
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            file.close();
            return line;
        }
    }
    file.close();
    return "";
}

void DataLogger::update(GSM &gsmModule) {
    // If nothing currently pending, pull next queued sample from SD.
    if (!_uploadPending) {
        if (_lastDataString.length() == 0) {
            String pending = _peekFirstPendingLine();
            if (pending.length() == 0) return;
            _lastDataString = pending;
        }
        _uploadPending = true;
        _uploadState = UPLOAD_IDLE;
    }

    if (_lastDataString.length() == 0) return;
    unsigned long now = millis();

    switch (_uploadState) {
        case UPLOAD_IDLE: {
            bool ok = _sendWithRetry(gsmModule, _buildUrl(API_KEY_1,
                { {"Temp", "field1"}, {"Hum", "field2"}, {"Press", "field3"},
                  {"Rain", "field4"}, {"WSpd", "field5"}, {"WDir", "field6"},
                  {"Light", "field7"}, {"SoilM", "field8"} }));
            if (ok) {
                _lastUploadTime = now;
                _uploadState = UPLOAD_WAIT_CH1;
            } else {
                _uploadPending = false;
            }
            break;
        }

        case UPLOAD_WAIT_CH1:
            if (now - _lastUploadTime >= TS_INTERVAL) _uploadState = UPLOAD_CH2;
            break;

        case UPLOAD_CH2: {
            bool ok = _sendWithRetry(gsmModule, _buildUrl(API_KEY_2,
                { {"V33", "field1"}, {"V5", "field2"}, {"VBatt", "field3"} }));
            if (ok) {
                _lastUploadTime = now;
                _uploadState = UPLOAD_WAIT_CH2;
            } else {
                _uploadPending = false;
            }
            break;
        }

        case UPLOAD_WAIT_CH2:
            if (now - _lastUploadTime >= TS_INTERVAL) _uploadState = UPLOAD_CH3;
            break;

        case UPLOAD_CH3: {
            bool ok = _sendWithRetry(gsmModule, _buildUrl(API_KEY_3,
                { {"VSol", "field1"}, {"VDC", "field2"}, {"Alt", "field3"} }));
            if (ok) {
                _deleteLineFromSD(_lastDataString);
                _lastDataString = "";
                _uploadPending = false;
                _uploadState = UPLOAD_IDLE;
            } else {
                _uploadPending = false;
            }
            break;
        }

        default:
            _uploadState = UPLOAD_IDLE;
            break;
    }
}

void DataLogger::_deleteLineFromSD(String uploadedLine) {
    if (!_sdAvailable) return;

    File src = SD.open(_fileName, FILE_READ);
    if (!src) return;
    File tmp = SD.open(TEMP_FILE, FILE_WRITE);
    if (!tmp) {
        src.close();
        return;
    }

    bool lineDeleted = false;
    while (src.available()) {
        String line = src.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (!lineDeleted && line == uploadedLine) {
            lineDeleted = true;
            continue;
        }
        tmp.println(line);
    }

    src.close();
    tmp.close();

    if (!SD.exists(TEMP_FILE)) {
        Serial.println("[DataLogger] Temp log file missing, keeping original log.");
        return;
    }

    SD.remove(_fileName);
    if (!SD.rename(TEMP_FILE, _fileName)) {
        Serial.println("[DataLogger] Failed to replace log file after upload.");
    }
}

void DataLogger::_rotateLogIfNeeded() {
    if (!_sdAvailable) return;
    File file = SD.open(_fileName, FILE_READ);
    if (!file) return;

    int lineCount = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) lineCount++;
    }
    file.close();

    if (lineCount < MAX_LOG_LINES) return;

    File src = SD.open(_fileName, FILE_READ);
    File tmp = SD.open(TEMP_FILE, FILE_WRITE);
    if (!src || !tmp) {
        if (src) src.close();
        if (tmp) tmp.close();
        return;
    }

    int skipLines = lineCount / 2;
    int skipped = 0;
    while (src.available()) {
        String line = src.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (skipped < skipLines) {
            skipped++;
            continue;
        }
        tmp.println(line);
    }
    src.close();
    tmp.close();
    SD.remove(_fileName);
    SD.rename(TEMP_FILE, _fileName);
}

void DataLogger::uploadLastDataToThingspeak(GSM &gsmModule) {
    if (_lastDataString == "") {
        Serial.println("No data to upload.");
        ErrorLogger::log(COMP_SD_CARD, ERR_SD_NO_DATA, "_lastDataString is empty");
        return;
    }
    _uploadPending = true;
    _uploadState = UPLOAD_IDLE;
    Serial.println("[DataLogger] Manual upload queued. Call update() in loop().");
}