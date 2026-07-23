#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "SensorData.h"
#include "GSM.h"

class DataLogger
{
public:
  DataLogger(int csPin);
  void begin();

  // Formats data into labeled string and saves to BOTH Archive and Queue
  void logSensorData(String timestamp, SensorData data);

  // Processes the queue, sending oldest data first
  void uploadPendingData(GSM &gsmModule, unsigned long startTimeMs, unsigned long tonLimitMs);

  // Processes the queue over WiFi, sending oldest data first. Returns true if successful or nothing to send.
  bool uploadPendingDataWiFi(unsigned long startTimeMs, unsigned long tonLimitMs);

  // Save GSM usage statistics to SD
  void logGSMStats(String timestamp, uint32_t sent, uint32_t received, uint32_t cycles);

  // Log carrier USSD text response to SD
  void logUSSDMessage(String timestamp, String message);

private:
  int _csPin;
  String _fileName;      // Archive file (permanent)
  String _queueFileName; // Queue file (temporary buffer)
  String _lastDataString;

  // Helper to extract value from "Label:Value" string
  String getValueFromLog(String logLine, String label);
  String jsonField(String key, String value);
  String getTimestampFromLog(String logLine);

  // New: Removes the first line from the queue file
  bool popQueue();
};

#endif