#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
public:
    GSM();
    void setupGSM();
    void connectGPRS();
    void sendThingSpeakRequest(String url);
    void checkNetworkHealth();

private:
    bool sendCommand(const String& command, int timeout, bool debug, const String& expectedResponse = "OK");
    String readResponse(int timeout, bool debug);
    bool verifyGSMCommunication();
    bool verifyNetworkRegistration();

    unsigned long networkLossStart;
};

#endif