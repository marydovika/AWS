#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
public:
    GSM();
    void setupGSM();
    void connectGPRS(); // New: Setup Internet
    // Sends data to a specific ThingSpeak URL
    int sendThingSpeakRequest(String url);

private:
    String readResponse(int timeout, boolean debug);
    void sendCommand(const String& command, int timeout, boolean debug);
};

#endif