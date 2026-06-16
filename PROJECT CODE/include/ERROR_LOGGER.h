#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <RTClib.h>

// ── Log filename on SD card ──────────────────────────────────────────────────
#define ERROR_LOG_FILE "/error_log.txt"

#define COMP_RTC              "RTC"
#define COMP_BME280           "BME280"
#define COMP_DHT22            "DHT22"
#define COMP_DS18B20          "DS18B20"
#define COMP_POWER_MONITOR    "POWER_MONITOR"
#define COMP_GSM              "GSM"
#define COMP_LORA             "LORA"
#define COMP_SD_CARD          "SD_CARD"
#define COMP_MEMORY           "MEMORY"
#define COMP_SIM              "SIM"
#define COMP_WIFI             "WIFI"
#define COMP_DAVIS            "DAVIS_RAIN"
#define COMP_WINDSPEED        "WINDSPEED"
#define COMP_MAIN             "MAIN_LOOP"

#define ERR_RTC_NOT_FOUND         "I2C_NOT_FOUND"
#define ERR_RTC_POWER_LOSS        "POWER_LOSS"
#define ERR_RTC_NTP_SYNC_FAIL     "NTP_SYNC_FAIL"

#define ERR_BME_NOT_FOUND         "I2C_NOT_FOUND"
#define ERR_BME_NAN_READ          "NAN_READ"

#define ERR_DHT_TEMP_FAIL         "TEMP_READ_FAIL"
#define ERR_DHT_HUM_FAIL          "HUM_READ_FAIL"

#define ERR_DS18B20_DISCONNECTED  "SENSOR_DISCONNECTED"

#define ERR_PWR_I2C_INCOMPLETE    "I2C_READ_INCOMPLETE"
#define ERR_PWR_READ_FAIL         "READ_FAIL"

#define ERR_GSM_HANDSHAKE_FAIL    "HANDSHAKE_FAIL"
#define ERR_GSM_CMD_TIMEOUT       "COMMAND_TIMEOUT"

#define ERR_LORA_SERIAL_INIT_FAIL "SERIAL_INIT_FAIL"
#define ERR_LORA_CMD_TIMEOUT      "COMMAND_TIMEOUT"

#define ERR_SD_MOUNT_FAIL         "MOUNT_FAIL"
#define ERR_SD_WRITE_FAIL         "WRITE_FAIL"
#define ERR_SD_PARSE_FAIL         "PARSE_FAIL"
#define ERR_SD_NO_DATA            "NO_DATA"

#define ERR_MEM_INIT_FAIL         "INIT_FAIL"
#define ERR_MEM_CREATE_FAIL       "FILE_CREATE_FAIL"
#define ERR_MEM_WRITE_FAIL        "FILE_WRITE_FAIL"

#define ERR_SIM_RESTART_FAIL      "MODEM_RESTART_FAIL"
#define ERR_SIM_NETWORK_TIMEOUT   "NETWORK_REG_TIMEOUT"
#define ERR_SIM_GPRS_FAIL         "GPRS_CONNECT_FAIL"
#define ERR_SIM_CONN_LOST         "CONNECTION_LOST"
#define ERR_SIM_HTTP_ERROR        "HTTP_ERROR_STATUS"

#define ERR_WIFI_TIMEOUT          "CONNECTION_TIMEOUT"

#define ERR_DAVIS_ISR_NULL        "ISR_BEFORE_INIT"

#define ERR_WIND_ISR_NULL         "ISR_BEFORE_INIT"
#define ERR_WIND_TIMEOUT          "SENSOR_TIMEOUT"
#define ERR_WIND_DIVIDE_ZERO      "DIVIDE_BY_ZERO"

#define ERR_MAIN_BME_NAN          "BME280_NAN_SUBSTITUTED"
#define ERR_MAIN_POWER_FAIL       "POWER_READ_FAIL"

class ErrorLogger {
public:
    static void begin(RTC_DS3231* rtc);
    static void log(const char* component,
                    const char* errorType,
                    const char* detail = "");
    static uint32_t errorCount();
    static void printLast(uint8_t n = 10);

private:
    static RTC_DS3231* _rtc;
    static uint32_t    _errorCount;
    static bool        _sdAvailable;

    static String _getTimestamp();
    static void   _writeToSD(const String& line);
    static void   _writeToSerial(const String& line);
};

#endif // ERROR_LOGGER_H

class ErrorLogger {
public:
    static void begin(RTC_DS3231* rtc);
    static void log(const char* component,
                    const char* errorType,
                    const char* detail = "");
    static uint32_t errorCount();
    static void printLast(uint8_t n = 10);

private:
    static RTC_DS3231* _rtc;
    static uint32_t    _errorCount;
    static bool        _sdAvailable;

    static String _getTimestamp();
    static void   _writeToSD(const String& line);
    static void   _writeToSerial(const String& line);
};

#endif // ERROR_LOGGER_H