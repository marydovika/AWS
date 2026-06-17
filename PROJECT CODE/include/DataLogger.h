#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <initializer_list>
#include <utility>
#include "SensorData.h"
#include "GSM.h"

enum UploadState {
    UPLOAD_IDLE,
    UPLOAD_WAIT_CH1,
    UPLOAD_CH2,
    UPLOAD_WAIT_CH2,
    UPLOAD_CH3
};

class DataLogger {
public:
    DataLogger(int csPin);
    void begin();
    
    // Formats data into labeled string and saves to SD
    void logSensorData(String timestamp, SensorData data);

    // Non-blocking upload state-machine (call every loop()).
    void update(GSM &gsmModule);
    
    // Legacy/manual trigger.
    void uploadLastDataToThingspeak(GSM &gsmModule);

    // Helper to extract value from "Label:Value" string
    String getValueFromLog(String logLine, String label);

private:
    int _csPin;
    String _fileName;
    String _lastDataString;
    bool _sdAvailable;

    UploadState _uploadState;
    unsigned long _lastUploadTime;
    bool _uploadPending;
    
    String _buildUrl(String apiKey,
        std::initializer_list<std::pair<String, String>> fields);
    bool _sendWithRetry(GSM &gsmModule, String url);
    void _deleteLineFromSD(String uploadedLine);
    void _rotateLogIfNeeded();
    String _peekFirstPendingLine();

    // ThingSpeak Config
    const String API_KEY_1 = "WL5ALGBAQDZV674Z"; 
    const String API_KEY_2 = "IKCHAI6ID958MEYG";
    const String API_KEY_3 = "IBK1KTD4E6A0CKZK";
};

#endif