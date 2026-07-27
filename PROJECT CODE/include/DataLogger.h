#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "SensorData.h"
#include "GSM.h"

// ThingSpeak API keys are defined in the DataLogger class

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

  // Converts "Tuesday, 2026-07-14 13:50:41" → "2026-07-14T13:50:41Z" for ThingSpeak
  String toISO8601(String timestamp);

  // Returns empty string for "nan" so ThingSpeak accepts the entry
  String formatField(String value);

  // Removes the first line from the queue file
  bool popQueue();

  // Appends a permanently-rejected log line to /rejected.txt
  void logRejected(String logLine);

  // ThingSpeak Config
  const String API_KEY_1 = "0UOU523VQPM2FZXJ";
  const String API_KEY_2 = "5N37KH8M7FCF5PU2";
  const String API_KEY_3 = "XH83XCG9LMURW45L";
};

#endif