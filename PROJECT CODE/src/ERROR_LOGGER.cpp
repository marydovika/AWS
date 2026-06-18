#include "ERROR_LOGGER.h"

RTC_DS3231* ErrorLogger::_rtc          = nullptr;
uint32_t    ErrorLogger::_errorCount   = 0;
bool        ErrorLogger::_sdAvailable  = false;

void ErrorLogger::begin(RTC_DS3231* rtc) {
    _rtc = rtc;

    File f = SD.open(ERROR_LOG_FILE, FILE_APPEND);
    if (f) {
        _sdAvailable = true;
        String marker = "\n";
        marker += "========================================\n";
        marker += "SESSION START  " + _getTimestamp() + "\n";
        marker += "========================================\n";
        f.print(marker);
        f.close();
        Serial.println("[ErrorLogger] Initialised. Logging to " ERROR_LOG_FILE);
    } else {
        _sdAvailable = false;
        Serial.println("[ErrorLogger] WARNING: SD card not available. "
                       "Errors will be logged to Serial only.");
    }
}

void ErrorLogger::log(const char* component,
                      const char* errorType,
                      const char* detail) {

    _errorCount++;

    String line = "[" + _getTimestamp() + "]";
    line += " [";
    line += component;
    line += "] [";
    line += errorType;
    line += "]";

    if (detail && strlen(detail) > 0) {
        line += " ";
        line += detail;
    }

    _writeToSerial(line);

    if (_sdAvailable) {
        _writeToSD(line);
    }
}

uint32_t ErrorLogger::errorCount() {
    return _errorCount;
}

void ErrorLogger::printLast(uint8_t n) {
    if (!_sdAvailable) {
        Serial.println("[ErrorLogger] SD not available — cannot read log.");
        return;
    }

    File f = SD.open(ERROR_LOG_FILE, FILE_READ);
    if (!f) {
        Serial.println("[ErrorLogger] Could not open log file for reading.");
        return;
    }

    String lines[n];
    uint8_t idx = 0;
    uint8_t count = 0;
    String current = "";

    while (f.available()) {
        char c = f.read();
        if (c == '\n') {
            if (current.length() > 0) {
                lines[idx % n] = current;
                idx++;
                count++;
            }
            current = "";
        } else {
            current += c;
        }
    }
    f.close();

    uint8_t total = (count < n) ? count : n;
    uint8_t start = (count < n) ? 0 : (idx % n);

    Serial.println("──── Last " + String(total) + " error log entries ────");
    for (uint8_t i = 0; i < total; i++) {
        Serial.println(lines[(start + i) % n]);
    }
    Serial.println("──────────────────────────────────────");
}

String ErrorLogger::_getTimestamp() {
    if (_rtc != nullptr) {
        DateTime now = _rtc->now();

        char buf[24];
        snprintf(buf, sizeof(buf),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(),  now.day(),
                 now.hour(), now.minute(), now.second());
        return String(buf);
    }

    unsigned long ms = millis();
    unsigned long secs  = ms / 1000;
    unsigned long mins  = secs / 60;
    unsigned long hours = mins / 60;

    char buf[24];
    snprintf(buf, sizeof(buf),
             "UPTIME %02luh%02lum%02lus",
             hours, mins % 60, secs % 60);
    return String(buf);
}

void ErrorLogger::_writeToSD(const String& line) {
    File f = SD.open(ERROR_LOG_FILE, FILE_APPEND);
    if (f) {
        f.println(line);
        f.close();
    } else {
        _sdAvailable = false;
        Serial.println("[ErrorLogger] SD write failed — switching to Serial only.");
    }
}

void ErrorLogger::_writeToSerial(const String& line) {
    Serial.println(line);
}