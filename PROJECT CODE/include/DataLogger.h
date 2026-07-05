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
    const String API_KEY_1 = "OBTHNU1FM6FDUI3H"; 
    const String API_KEY_2 = "Z44BOF91AKPXF4Z3";
    const String API_KEY_3 = "W1DYIKRSAP3M0J7H";
};

#endif