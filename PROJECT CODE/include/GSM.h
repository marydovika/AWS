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

  public:
    // ── Internal helpers ──────────────────────────────────
    void sendCommand(const String& command, int timeout, boolean debug);
    String sendCommandWithResponse(const String& command, int timeout, boolean debug);
    void countSent(const String& s);
    void countReceived(const String& s);
};

#endif