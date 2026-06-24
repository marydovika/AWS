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
    bool waitForNetwork(int timeoutMs);
    void initSerial();

    // ── Data sending ──────────────────────────────────────
    // Old: ThingSpeak (keeping in case needed)
    bool sendThingSpeakRequest(String url);

    // New: Post raw ESP32 string to Django ingest endpoint
    void postToDjango(String jsonPayload);

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

    // HTTP POST to Django
    bool postToDjango(const String& json);
    const String DJANGO_URL = "http://184.106.153.149/api/weather/"; // Replace with your Django server endpoint

  public:
    // ── Internal helpers ──────────────────────────────────
    void sendCommand(const String& command, int timeout, boolean debug);
    String sendCommandWithResponse(const String& command, int timeout, boolean debug);
    void countSent(const String& s);
    void countReceived(const String& s);
    String decodeUCS2(const String& hexStr);
};

#endif