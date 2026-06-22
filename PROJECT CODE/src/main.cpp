/**
 * main.cpp — Multi-Level Fixed Duty Cycling Implementation
 * =========================================================
 * Strategy:
 *   UPDATE_INTERVAL_MINUTES = 10 minutes default (configurable via web portal)
 *   TON  = 2 minutes  (active window: sensors + log + transmit)
 *   TOFF = Dynamic (Total Cycle - Active Time)
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "DHT22.h"
#include "AIR_PRESSURE.h"
#include "LIGHT_SENSOR.h"
#include "SOILMOISTURE.h"
#include "RTC.h"
#include "GSM.h"
#include "DAVIS.h"
#include "WINDSPEED.h"
#include "WIND_DIRECTION.h"
#include "POWER_MONITORING.h"
#include "DataLogger.h"
#include "SensorData.h"
#include "LORA.h"

// ── Duty cycle config (defaults match original code) ─────
static uint32_t UPDATE_INTERVAL_MINUTES = 10; // overwritten from flash if set
static const uint64_t TON_MS = 2ULL * 60ULL * 1000ULL; // 2 min active window

// ── Config portal ─────────────────────────────────────────
#define PORTAL_TIMEOUT_MS  120000   // 2 minutes portal window per wake
const char* AP_SSID     = "AWS-WIFI-2026";
const char* AP_PASSWORD = "aws-2026@!";
WebServer server(80);
Preferences prefs;
uint32_t savedInterval = 10; // loaded from flash

// ── Hardware ──────────────────────────────────────────────
#define GSM_POWER_PIN  32
static const uint32_t GSM_WARMUP_MS = 3000;

extern HardwareSerial SerialL;

DHTSensor            dhtsensor;
AirPressure          airpressure;
LightSensor          lightsensor;
Soilmoisture         soilmoisture;
Rtc                  rtc1;
Davis                davisrain;
WindSpeedSensor      windspeedsensor;
WindDirectionSensor  winddirectionsensor;
PowerMonitoring      powermonitoring;
GSM                  simmodule;
Lora                 loramodule;
DataLogger           dataLogger(4);

// ── Forward declarations ──────────────────────────────────
String buildPage(String message = "");
void handleRoot();
void handleSave();
void handleNotFound();
void runPortalWindow();

// ─────────────────────────────────────────────────────────
// Web portal HTML
// ─────────────────────────────────────────────────────────
String buildPage(String message) {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WIMEA-AWS Config</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #EBF4FB;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .card {
      background: white;
      border-radius: 14px;
      border: 1px solid #D8E4EF;
      padding: 32px 28px;
      width: 100%;
      max-width: 420px;
      box-shadow: 0 4px 24px rgba(0,0,0,0.08);
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 24px;
      padding-bottom: 18px;
      border-bottom: 1px solid #EBF4FB;
    }
    .brand-icon {
      width: 36px; height: 36px;
      background: #1A3A5C;
      border-radius: 8px;
      display: flex; align-items: center; justify-content: center;
      color: white; font-size: 18px;
    }
    .brand-name { font-size: 15px; font-weight: 700; color: #1A3A5C; }
    .brand-sub  { font-size: 11px; color: #8A9BB0; }
    h1 { font-size: 18px; font-weight: 600; color: #1A2332; margin-bottom: 4px; }
    .subtitle { font-size: 13px; color: #5C6E82; margin-bottom: 22px; }
    label {
      display: block;
      font-size: 12.5px;
      font-weight: 600;
      color: #1A2332;
      margin-bottom: 6px;
    }
    input[type="number"] {
      width: 100%;
      padding: 10px 14px;
      border: 1px solid #D8E4EF;
      border-radius: 8px;
      font-size: 14px;
      color: #1A2332;
      background: #F8FAFC;
      margin-bottom: 6px;
      outline: none;
    }
    input:focus { border-color: #0A6EBD; box-shadow: 0 0 0 3px rgba(10,110,189,0.1); }
    .hint { font-size: 11.5px; color: #8A9BB0; margin-bottom: 20px; }
    button {
      width: 100%;
      padding: 12px;
      background: #1A3A5C;
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
    }
    button:hover { background: #0A6EBD; }
    .success {
      background: #E8F5E9;
      border: 1px solid #A5D6A7;
      border-radius: 8px;
      padding: 10px 14px;
      font-size: 13px;
      color: #2E7D32;
      margin-bottom: 18px;
    }
    .current {
      background: #F2F6FA;
      border-radius: 8px;
      padding: 12px 14px;
      margin-bottom: 20px;
      font-size: 12.5px;
      color: #5C6E82;
      line-height: 1.7;
    }
    .current strong { color: #1A2332; }
    .warn {
      background: #FFF8E1;
      border: 1px solid #FFE082;
      border-radius: 8px;
      padding: 10px 14px;
      font-size: 12px;
      color: #7B5800;
      margin-top: 16px;
    }
    .station-id {
      font-size: 11px;
      color: #B0C4D8;
      text-align: center;
      margin-top: 18px;
    }
  </style>
</head>
<body>
<div class="card">
  <div class="brand">
    <div class="brand-icon">&#9729;</div>
    <div>
      <div class="brand-name">AWS</div>
      <div class="brand-sub">Automatic Weather Station</div>
    </div>
  </div>

  <h1>Sleep Cycle Configuration</h1>
  <p class="subtitle">
    Set how many minutes the station sleeps between readings.
    The active window is always 2 minutes.
  </p>
)rawhtml";

  if (message != "") {
    html += "<div class='success'>&#10003; " + message + "</div>";
  }

  // Show current running config
  uint32_t toff = UPDATE_INTERVAL_MINUTES >= 2
                  ? UPDATE_INTERVAL_MINUTES - 2
                  : 0;
  html += "<div class='current'>"
          "Current cycle: <strong>" + String(UPDATE_INTERVAL_MINUTES) + " min total</strong><br>"
          "Active (TON): <strong>2 min</strong> &nbsp;&middot;&nbsp; "
          "Sleep (TOFF): <strong>~" + String(toff) + " min</strong>"
          "</div>";

  html += R"rawhtml(
  <form method="POST" action="/save">
    <label for="interval">Total cycle interval (minutes)</label>
    <input type="number" name="interval" id="interval"
           min="3" max="60" value=")rawhtml";
  html += String(UPDATE_INTERVAL_MINUTES);
  html += R"rawhtml(" required>
    <p class="hint">
      Min: 3 min &nbsp;&middot;&nbsp; Max: 60 min &nbsp;&middot;&nbsp;
      Default: 10 min<br>
      Active window is fixed at 2 min. Sleep = Interval &minus; Active time.
    </p>
    <button type="submit">&#128190; Save configuration</button>
  </form>

  <div class="warn">
    &#9888; Setting takes effect on the <strong>next wake cycle</strong>.
    The station will sleep after this portal window closes.
  </div>

  <p class="station-id">Station ID: AWS-UG-001 &nbsp;&middot;&nbsp; Firmware v1.1</p>
</div>
</body>
</html>
)rawhtml";

  return html;
}

// ─────────────────────────────────────────────────────────
// Route handlers
// ─────────────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleSave() {
  if (server.method() != HTTP_POST) {
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  uint32_t requested = server.arg("interval").toInt();

  // Clamp to safe range — active window is 2 min so minimum total is 3
  if (requested < 3)  requested = 3;
  if (requested > 60) requested = 60;

  UPDATE_INTERVAL_MINUTES = requested;
  savedInterval = requested;

  prefs.begin("awsconfig", false);
  prefs.putUInt("interval", requested);
  prefs.end();

  Serial.println("[Portal] Saved interval: " + String(requested) + " min");

  String msg = "Saved! Cycle set to " + String(requested)
             + " min (sleep ~" + String(requested - 2) + " min).";
  server.send(200, "text/html", buildPage(msg));
}

void handleNotFound() {
  // Captive portal redirect — catches any domain the phone tries
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "Redirecting to config portal...");
}

// ─────────────────────────────────────────────────────────
// Portal window — runs for PORTAL_TIMEOUT_MS then returns
// ─────────────────────────────────────────────────────────
void runPortalWindow() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.print("[Portal] AP IP: ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1

  server.on("/",     HTTP_GET,  handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("[Portal] Open for " + String(PORTAL_TIMEOUT_MS / 1000) + "s — SSID: " + AP_SSID);

  unsigned long start = millis();
  while (millis() - start < PORTAL_TIMEOUT_MS) {
    server.handleClient();
    delay(10);
  }

  // Tear down WiFi cleanly before sleep to save power
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[Portal] Window closed, WiFi off.");
}

// ─────────────────────────────────────────────────────────
// Original helper functions (unchanged)
// ─────────────────────────────────────────────────────────
float computeAvgPower(float duty, float pActive_mW, float pSleep_mW) {
    return (duty * pActive_mW) + ((1.0f - duty) * pSleep_mW);
}

void logDebugEvent(const String &event) {
    if (!SD.begin(4)) return;
    File f = SD.open("/sleep_debug.txt", FILE_APPEND);
    if (!f) return;
    f.println(String(millis()) + "," + event);
    f.close();
}

void gsmPowerOn() {
    Serial.println("[DC] GSM Power Gate: ON");
    digitalWrite(GSM_POWER_PIN, HIGH);
    delay(GSM_WARMUP_MS);
    simmodule.setupGSM();
}

void gsmPowerOff() {
    Serial.println("[DC] GSM Power Gate: OFF");
    digitalWrite(GSM_POWER_PIN, LOW);
}

void loraSleep() {
    Serial.println("[DC] LoRa: AT+LOWPOWER");
    while (SerialL.available()) SerialL.read();
    SerialL.println("AT+LOWPOWER");
    delay(200);
}

void loraWake() {
    Serial.println("[DC] LoRa: waking");
    SerialL.println("AT");
    delay(300);
    while (SerialL.available()) SerialL.read();
}

void sdCardRelease() {
    SD.end();
    SPI.end();
    Serial.println("[DC] SD SPI bus released");
}

void enterDeepSleep(unsigned long tonStart) {
    detachInterrupt(digitalPinToInterrupt(25));
    detachInterrupt(digitalPinToInterrupt(33));
    delay(50);

    uint64_t totalCycleUs = (uint64_t)UPDATE_INTERVAL_MINUTES * 60ULL * 1000000ULL;
    uint64_t activeUs     = (uint64_t)(millis() - tonStart) * 1000ULL;

    // Guard: if active ran over the cycle window, sleep minimum 10 seconds
    uint64_t sleepUs = (totalCycleUs > activeUs)
                       ? (totalCycleUs - activeUs)
                       : (10ULL * 1000000ULL);

    Serial.printf("[DC] Total Active Time : %lu ms\n", (unsigned long)(activeUs  / 1000ULL));
    Serial.printf("[DC] Deep Sleep Duration: %lu ms\n", (unsigned long)(sleepUs / 1000ULL));
    Serial.flush();

    esp_sleep_enable_timer_wakeup(sleepUs);
    esp_deep_sleep_start();
}

SensorData readAllSensors() {
    SensorData data;
    data.airPressure = airpressure.readPressure();
    data.altitude    = airpressure.readAltitude(1013.25);
    data.temperature = airpressure.readTemperature();
    data.humidity    = airpressure.readHumidity();

    if (powermonitoring.readData()) {
        VoltageData v    = powermonitoring.getData();
        data.volt_3v3    = v.v1; data.volt_5v    = v.v2; data.volt_batt  = v.v3;
        data.volt_solar  = v.v4; data.volt_dc     = v.v5; data.curr_batt  = v.v6;
        data.curr_solar  = v.v7;
    } else {
        data.volt_3v3 = data.volt_5v   = data.volt_batt  = 0.0f;
        data.volt_solar = data.volt_dc = data.curr_batt  = data.curr_solar = 0.0f;
    }

    data.lightLevel    = lightsensor.readLightLevel();
    data.soilMoisture  = soilmoisture.readSoilMoisture();
    data.rainCount     = davisrain.readRainGauge();
    data.windSpeed     = windspeedsensor.readWindSpeedKPH();
    data.windDirection = winddirectionsensor.readWindDirectionDeg();

    return data;
}

void logToSD(SensorData &data) {
    Serial.println("[DC] === SD Log Phase ===");
    String timeStr = String(rtc1.getDateTime().c_str());
    dataLogger.logSensorData(timeStr, data);
}

void transmitData(SensorData &data, unsigned long tonStart) {
    Serial.println("[DC] === TX Phase ===");
    loraWake();
    String payload = "T:" + String(data.temperature, 1) + ",H:" + String(data.humidity, 1);
    String atCmd   = "AT+DTRX=0,1," + String(payload.length()) + "," + payload;
    loramodule.sendData(atCmd, 3000);
    loraSleep();

    gsmPowerOn();

    String netTime = simmodule.getNetworkTime();
    if (netTime != "") {
        rtc1.syncWithGSM(netTime);
    }

    dataLogger.uploadPendingData(simmodule, tonStart, TON_MS);

    Serial.println("\n[GSM] === Data Usage Stats ===");
    Serial.printf("[GSM] Total Sent    : %u bytes\n",  simmodule.getTotalBytesSent());
    Serial.printf("[GSM] Total Received: %u bytes\n",  simmodule.getTotalBytesReceived());
    uint32_t total = simmodule.getTotalBytesSent() + simmodule.getTotalBytesReceived();
    Serial.printf("[GSM] Total Traffic : %u bytes (%.2f KB)\n", total, total / 1024.0);
    Serial.printf("[GSM] Total Cycles  : %u\n", simmodule.getCycleCount());
    if (simmodule.getCycleCount() > 0)
        Serial.printf("[GSM] Avg per Cycle : %.2f KB\n", (total / 1024.0) / simmodule.getCycleCount());
    Serial.println("[GSM] ========================\n");

    dataLogger.logGSMStats(rtc1.getDateTime().c_str(),
                           simmodule.getTotalBytesSent(),
                           simmodule.getTotalBytesReceived(),
                           simmodule.getCycleCount());
    gsmPowerOff();
    Serial.println("[DC] TX Phase complete.");
}

// ─────────────────────────────────────────────────────────
// Setup — everything happens here, loop() is empty
// ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(9600);
    delay(200);

    Wire.setTimeOut(50);

    // ── Wake / reset diagnostics (unchanged) ──────────────
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    esp_reset_reason_t       reset_reason  = esp_reset_reason();

    Serial.println("\n[DC] ================================");
    Serial.print("[DC] Wakeup reason: ");
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("External signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("External signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP:      Serial.println("ULP program"); break;
        default: Serial.printf("Not caused by deep sleep (%d)\n", wakeup_reason); break;
    }
    Serial.print("[DC] Reset reason: ");
    switch (reset_reason) {
        case ESP_RST_POWERON:   Serial.println("Power-on"); break;
        case ESP_RST_EXT:       Serial.println("External pin"); break;
        case ESP_RST_SW:        Serial.println("Software"); break;
        case ESP_RST_PANIC:     Serial.println("Exception/Panic"); break;
        case ESP_RST_INT_WDT:   Serial.println("Interrupt Watchdog"); break;
        case ESP_RST_TASK_WDT:  Serial.println("Task Watchdog"); break;
        case ESP_RST_WDT:       Serial.println("Other Watchdog"); break;
        case ESP_RST_DEEPSLEEP: Serial.println("Deep Sleep"); break;
        case ESP_RST_BROWNOUT:  Serial.println("Brownout"); break;
        case ESP_RST_SDIO:      Serial.println("SDIO"); break;
        default:                Serial.println("Unknown"); break;
    }
    Serial.println("[DC] ================================");

    // ── Load saved interval from flash ────────────────────
    prefs.begin("awsconfig", true);
    savedInterval = prefs.getUInt("interval", 10); // default: 10 min
    prefs.end();
    UPDATE_INTERVAL_MINUTES = savedInterval;
    Serial.println("[Portal] Loaded interval: " + String(UPDATE_INTERVAL_MINUTES) + " min");

    // ── Hardware init ─────────────────────────────────────
    pinMode(GSM_POWER_PIN, OUTPUT);
    digitalWrite(GSM_POWER_PIN, LOW);

    powermonitoring.begin(21, 22);
    delay(100);

    rtc1.setupRTC();
    dhtsensor.getsensor();
    airpressure.sensor_setup();
    davisrain.setupRainGauge();
    windspeedsensor.setupSensor();
    winddirectionsensor.setupSensor();
    lightsensor.setupSensor();
    soilmoisture.setupSensor();
    dataLogger.begin();
    loramodule.setupLora();

    // ── Config portal window ──────────────────────────────
    // Runs for PORTAL_TIMEOUT_MS (15s) every wake.
    // User can connect to WIMEA-AWS-001 and change the interval.
    // If nobody connects, it exits automatically and we proceed.
    runPortalWindow();

    // ── TON start: mark active window after portal ────────
    unsigned long tonStart = millis();

    // ── Sensor read, log, transmit (unchanged) ────────────
    SensorData currentData = readAllSensors();
    logToSD(currentData);
    transmitData(currentData, tonStart);

    Serial.printf("[DC] Active window closed. Elapsed: %lu ms\n", millis() - tonStart);

    sdCardRelease();

    Serial.println("[DC] Sleep phase: entering deep sleep");
    enterDeepSleep(tonStart);
}

void loop() {}