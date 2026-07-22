#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

// ── Django ingest endpoints (Cloudflare Tunnel) ──
// Update BASE_URL only — the 3 endpoints below stay in sync automatically
#define DJANGO_BASE_URL     "https://muscles-compete-milton-reproduce.trycloudflare.com"
#define DJANGO_WEATHER_URL  DJANGO_BASE_URL "/api/ingest/weather/"
#define DJANGO_VOLTAGE_URL  DJANGO_BASE_URL "/api/ingest/voltage/"
#define DJANGO_CURRENT_URL  DJANGO_BASE_URL "/api/ingest/current/"

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
    bool postToDjango(String url, String jsonPayload);
    
    void checkNetworkHealth();
    bool verifyGSMCommunication();
    bool verifyNetworkRegistration();
   
    // ── Network time ──────────────────────────────────────
    String getNetworkTime();
     String queryUSSD(const String& ussdCode, int timeoutMs);
    String extractUSSDMessage(const String& cusdResponse);
    String decodeUCS2(const String& hexStr);
    float parseBalanceFromUSSD(const String& msg);

    // ── Byte usage tracking ───────────────────────────────
    uint32_t getTotalBytesSent();
    uint32_t getTotalBytesReceived();
    uint32_t getCycleCount();
    void resetByteCounters();

  
    // ── Internal helpers ──────────────────────────────────
    bool sendCommand(const String& command, int timeout, bool debug, const String& expectedResponse);
    void sendCommand(const String& command, int timeout, bool debug);
    String sendCommandWithResponse(const String& command, int timeout, bool debug);
    String readResponse(int timeout, bool debug, const String& expectedResponse = "");
    void countSent(const String& s);
    void countReceived(const String& s);

  private:
    unsigned long networkLossStart;
};

#endif