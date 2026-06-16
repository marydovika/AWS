#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <RTClib.h>

// ── Log filename on SD card ──────────────────────────────────────────────────
#define ERROR_LOG_FILE "/error_log.txt"