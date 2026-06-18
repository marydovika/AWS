#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "SensorData.h"
#include "GSM.h"

// ThingSpeak API keys
#define API_KEY_1 "YOUR_KEY_1"
#define API_KEY_2 "YOUR_KEY_2"
#define API_KEY_3 "YOUR_KEY_3"

class DataLogger {
  public:
    DataLogger(int csPin);
    void begin();
    void logSensorData(String timestamp, SensorData data);

    // ← these three were missing entirely
    void uploadPendingData(GSM &gsmModule, unsigned long startTimeMs, unsigned long tonLimitMs);
    void logGSMStats(String timestamp, uint32_t sent, uint32_t received, uint32_t cycles);
    bool popQueue();

  private:
    int _csPin;
    String _fileName;
    String _queueFileName;   // ← this was missing
    String _lastDataString;
    String getValueFromLog(String logLine, String label);
};

#endif