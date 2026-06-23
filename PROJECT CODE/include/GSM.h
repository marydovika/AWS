#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
  public:
    GSM();
    void setupGSM();
    bool sendThingSpeakRequest(String url);  // ← was void, must be bool
    bool sendCommand(const String& command, int timeout, boolean debug);
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