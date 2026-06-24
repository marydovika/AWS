#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
public:
    GSM();
    void setupGSM();
    void connectGPRS(); // New: Setup Internet
    bool waitForNetwork(int timeoutMs); // New: Wait for cell signal
    // Sends data to a specific ThingSpeak URL
    bool sendThingSpeakRequest(String url); 

    // New: Get network time from GSM module
    String getNetworkTime();

    // Byte counter for data estimation
    uint32_t getTotalBytesSent();
    uint32_t getTotalBytesReceived();
    uint32_t getCycleCount();
    void resetByteCounters();

    // USSD and Balance Checking
    String queryUSSD(const String& ussdCode, int timeoutMs);
    String extractUSSDMessage(const String& cusdResponse);
    float parseBalanceFromUSSD(const String& msg);

    // HTTP POST to Django
    bool postToDjango(const String& json);
    const String DJANGO_URL = "http://184.106.153.149/api/weather/"; // Replace with your Django server endpoint

private:
    void sendCommand(const String& command, int timeout, boolean debug);
    String sendCommandWithResponse(const String& command, int timeout, boolean debug);
    void countSent(const String& s);
    void countReceived(const String& s);
    String decodeUCS2(const String& hexStr);
};

#endif