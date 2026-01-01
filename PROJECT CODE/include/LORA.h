#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

class Lora {
public:
    Lora();
    void setupLora();
    void sendData(const String& data, int timeout = 2000);  
private:
    // Add private members if needed
};
