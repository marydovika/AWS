#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
  public:
    // ── Constructor ───────────────────────────────────────
    GSM();

    // ── Setup ─────────────────────────────────────────────
    void setupGSM();
    void connectGPRS();
    void disconnectGPRS();
    bool waitForNetwork(int timeoutMs);
    void initSerial();

    // ── Data sending ──────────────────────────────────────
    // Old: ThingSpeak (keeping in case needed)
    bool sendThingSpeakRequest(String url);

    // New: Post raw ESP32 string to Django ingest endpoint
    void postToDjango(String jsonPayload);
    const String DJANGO_URL = "https://aws-web-app-lsix.onrender.com/api/ingest/";

    // ── Network time ──────────────────────────────────────
    String getNetworkTime();

    // ── Byte usage tracking ───────────────────────────────
    uint32_t getTotalBytesSent();
    uint32_t getTotalBytesReceived();
    uint32_t getCycleCount();
    void resetByteCounters();

    // USSD and Balance Checking
    String queryUSSD(const String& ussdCode, int timeoutMs);
    String extractUSSDMessage(const String& cusdResponse);
    float parseBalanceFromUSSD(const String& msg);

    // ── Internal helpers ──────────────────────────────────
    void sendCommand(const String& command, int timeout, boolean debug);
    bool sendCommand(const String& command, int timeout, bool debug, const String& expectedResponse);
    String sendCommandWithResponse(const String& command, int timeout, boolean debug);
    String readResponse(int timeout, bool debug, const String& expectedResponse = "");
    void countSent(const String& s);
    void countReceived(const String& s);
    String decodeUCS2(const String& hexStr);
    void checkNetworkHealth();
    bool verifyGSMCommunication();
    bool verifyNetworkRegistration();

  private:
    unsigned long networkLossStart;
};

#endif