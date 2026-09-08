<<<<<<< HEAD
#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "SensorData.h"
#include "GSM.h"

class DataLogger {
public:
    DataLogger(int csPin);
    void begin();
    
    // Formats data into labeled string and saves to SD
    void logSensorData(String timestamp, SensorData data);
    
    // Reads the last written line and sends to ThingSpeak via GSM
    void uploadLastDataToThingspeak(GSM &gsmModule);

private:
    int _csPin;
    String _fileName;
    String _lastDataString; // Caches the last written line for efficiency

    // Helper to extract value from "Label:Value" string
    String getValueFromLog(String logLine, String label);
    
    // ThingSpeak Config (Update these)
    const String API_KEY_1 = "WL5ALGBAQDZV674Z"; 
    const String API_KEY_2 = "IKCHAI6ID958MEYG";
    const String API_KEY_3 = "IBK1KTD4E6A0CKZK";
};

=======
#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "SensorData.h"
#include "GSM.h"

class DataLogger {
public:
    DataLogger(int csPin);
    void begin();
    
    // Formats data into labeled string and saves to SD
    void logSensorData(String timestamp, SensorData data);
    
    // Reads the last written line and sends to ThingSpeak via GSM
    void uploadLastDataToThingspeak(GSM &gsmModule);

private:
    int _csPin;
    String _fileName;
    String _lastDataString; // Caches the last written line for efficiency

    // Helper to extract value from "Label:Value" string
    String getValueFromLog(String logLine, String label);
    
    // ThingSpeak Config (Update these)
    const String API_KEY_1 = "WL5ALGBAQDZV674Z"; 
    const String API_KEY_2 = "IKCHAI6ID958MEYG";
    const String API_KEY_3 = "IBK1KTD4E6A0CKZK";
};

>>>>>>> 0a3d4a415a0e6c63afa63b2638ea703b80302390
#endif