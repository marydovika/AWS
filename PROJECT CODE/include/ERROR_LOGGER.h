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