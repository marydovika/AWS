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