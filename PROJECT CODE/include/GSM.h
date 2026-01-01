#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

class GSM {
public:
    GSM();
    void setupGSM();
    void sendData(const String& data, int timeout = 2000);  
private:
    // Add private members if needed
};
