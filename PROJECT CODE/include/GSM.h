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

private:
    void sendCommand(const String& command, int timeout, boolean debug);
    String sendCommandWithResponse(const String& command, int timeout, boolean debug);
    void countSent(const String& s);
    void countReceived(const String& s);
    String getNetworkTime();

    // ← all of these were missing
    void countSent(const String& s);
    void countReceived(const String& s);
    uint32_t getTotalBytesSent();
    uint32_t getTotalBytesReceived();
    uint32_t getCycleCount();
    void resetByteCounters();

  private:
    void connectGPRS();
};

#endif