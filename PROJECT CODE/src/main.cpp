#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "WIFI_CONNECTION.h"
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


RTC_DATA_ATTR uint32_t updateIntervalMinutes = 10;
RTC_DATA_ATTR uint32_t cyclesSinceLastUSSD = 9999; // Initialize to high to force first-time check

// Data Bundle Config (Estimated vs Carrier USSD)
static const uint32_t TOTAL_BUNDLE_BYTES = 100UL * 1024UL * 1024UL; // 100MB Monthly Bundle (Airtel Uganda)
static const float USSD_THRESHOLD_PCT = 20.0f; // 20% limit
static const String USSD_CODE = "*131#"; // Airtel Uganda Balance Code

static const uint64_t TON_MS  = 4ULL * 60ULL * 1000ULL; // max active window

// Deep sleep schedule uses UPDATE_INTERVAL_MINUTES as total cycle time.
// TON itself is dynamically shortened inside uploadPendingData() when the queue is empty.


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
    // Detach FIRST — before any other teardown
    detachInterrupt(digitalPinToInterrupt(25));
    detachInterrupt(digitalPinToInterrupt(33));
    delay(50); // Let any in-flight ISR finish

    uint64_t totalCycleUs = (uint64_t)updateIntervalMinutes * 60ULL * 1000000ULL;
    uint64_t activeUs = (uint64_t)(millis() - tonStart) * 1000ULL;
    uint64_t sleepUs = (totalCycleUs > activeUs) ? (totalCycleUs - activeUs) : (10ULL * 1000000ULL);

    Serial.printf("[DC] Total Active Time: %lu ms\n", (unsigned long)(activeUs / 1000ULL));
    Serial.printf("[DC] Deep Sleep Duration: %lu ms\n", (unsigned long)(sleepUs / 1000ULL));
    Serial.flush(); // Ensure serial output completes before sleep

    esp_sleep_enable_timer_wakeup(sleepUs);
    esp_deep_sleep_start();
}

SensorData readAllSensors() {
    SensorData data;
    //Serial.println("[DC] readAllSensors: before airPressure.readPressure");
    data.airPressure = airpressure.readPressure();
    //Serial.println("[DC] readAllSensors: after airPressure.readPressure");

    //Serial.println("[DC] readAllSensors: before airPressure.readAltitude");
    data.altitude = airpressure.readAltitude(1013.25);
    //Serial.println("[DC] readAllSensors: after airPressure.readAltitude");

    //Serial.println("[DC] readAllSensors: before airPressure.readTemperature");
    data.temperature = airpressure.readTemperature();
    //Serial.println("[DC] readAllSensors: after airPressure.readTemperature");

    //Serial.println("[DC] readAllSensors: before airPressure.readHumidity");
    data.humidity = airpressure.readHumidity();
    //Serial.println("[DC] readAllSensors: after airPressure.readHumidity");

    //Serial.println("[DC] readAllSensors: before powermonitoring.readData");
    if (powermonitoring.readData()) {
        Serial.println("[DC] readAllSensors: power monitoring ok");
        VoltageData v = powermonitoring.getData();
        data.volt_3v3   = v.v1; data.volt_5v = v.v2; data.volt_batt = v.v3;
        data.volt_solar = v.v4; data.volt_dc = v.v5; data.curr_batt = v.v6;
        data.curr_solar = v.v7;
    } else {
        Serial.println("[DC] readAllSensors: power monitoring failed");
        data.volt_3v3 = data.volt_5v = data.volt_batt = 0.0f;
        data.volt_solar = data.volt_dc = data.curr_batt = data.curr_solar = 0.0f;
    }

    //Serial.println("[DC] readAllSensors: before lightsensor.readLightLevel");
    data.lightLevel    = lightsensor.readLightLevel();
    //Serial.println("[DC] readAllSensors: after lightsensor.readLightLevel");

    //Serial.println("[DC] readAllSensors: before soilmoisture.readSoilMoisture");
    data.soilMoisture  = soilmoisture.readSoilMoisture();
    //Serial.println("[DC] readAllSensors: after soilmoisture.readSoilMoisture");

    //Serial.println("[DC] readAllSensors: before davisrain.readRainGauge");
    data.rainCount     = davisrain.readRainGauge();
    //Serial.println("[DC] readAllSensors: after davisrain.readRainGauge");

    //Serial.println("[DC] readAllSensors: before windspeedsensor.readWindSpeedKPH");
    data.windSpeed     = windspeedsensor.readWindSpeedKPH();
    //Serial.println("[DC] readAllSensors: after windspeedsensor.readWindSpeedKPH");

    //Serial.println("[DC] readAllSensors: before winddirectionsensor.readWindDirectionDeg");
    data.windDirection = winddirectionsensor.readWindDirectionDeg();
    //Serial.println("[DC] readAllSensors: after winddirectionsensor.readWindDirectionDeg");

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
    String atCmd = "AT+DTRX=0,1," + String(payload.length()) + "," + payload;
    loramodule.sendData(atCmd, 3000);
    loraSleep();

    gsmPowerOn();
    
    // SYNC EARLY: Get network time as soon as GPRS is connected
    String networkTime = simmodule.getNetworkTime();
    if (networkTime != "") {
        rtc1.syncWithGSM(networkTime);
    }

    // Hybrid USSD Balance Check
    uint32_t totalUsed = simmodule.getTotalBytesSent() + simmodule.getTotalBytesReceived();
    float pctRemaining = (1.0f - ((float)totalUsed / (float)TOTAL_BUNDLE_BYTES)) * 100.0f;
    if (pctRemaining < 0.0f) pctRemaining = 0.0f;

    Serial.printf("[GSM] Soft-tracked Data Usage: %lu bytes used of %lu (%.2f%% remaining)\n", 
                  (unsigned long)totalUsed, (unsigned long)TOTAL_BUNDLE_BYTES, pctRemaining);

    // Run USSD check only when remaining drops below threshold AND at most once per 24 hours
    uint32_t cyclesInADay = (24 * 60) / updateIntervalMinutes;
    if (cyclesInADay == 0) cyclesInADay = 1; // Prevent division by zero

    if (pctRemaining <= USSD_THRESHOLD_PCT && cyclesSinceLastUSSD >= cyclesInADay) {
        Serial.printf("[GSM] Data bundle estimate < %.1f%%. Triggering carrier USSD validation...\n", USSD_THRESHOLD_PCT);
        String ussdResponse = simmodule.queryUSSD(USSD_CODE, 15000);
        String cleanMsg = simmodule.extractUSSDMessage(ussdResponse);
        
        Serial.println("[GSM] Raw USSD Response: " + ussdResponse);
        Serial.println("[GSM] Cleaned USSD Message: " + cleanMsg);
        
        // Log clean message to SD card
        dataLogger.logUSSDMessage(rtc1.getDateTime().c_str(), cleanMsg);
        
        // Self-healing balance reset if recharged
        float actualMB = simmodule.parseBalanceFromUSSD(cleanMsg);
        if (actualMB >= 0.0f) {
            float actualBytes = actualMB * 1024.0f * 1024.0f;
            float actualPct = (actualBytes / (float)TOTAL_BUNDLE_BYTES) * 100.0f;
            Serial.printf("[GSM] Carrier Balance: %.2f MB (%.2f%% remaining)\n", actualMB, actualPct);
            
            if (actualPct > USSD_THRESHOLD_PCT) {
                Serial.println("[GSM] Carrier reports data has been replenished. Resetting local counters!");
                simmodule.resetByteCounters();
            }
        }
        cyclesSinceLastUSSD = 0;
    } else {
        cyclesSinceLastUSSD++;
        Serial.printf("[GSM] Cycles since last USSD check: %lu (Next check in %lu cycles)\n", 
                      (unsigned long)cyclesSinceLastUSSD, 
                      (unsigned long)(cyclesSinceLastUSSD >= cyclesInADay ? 0 : cyclesInADay - cyclesSinceLastUSSD));
    }

    dataLogger.uploadPendingData(simmodule, tonStart, TON_MS);

    // Display Byte Usage
    Serial.println("\n[GSM] === Data Usage Stats ===");
    Serial.printf("[GSM] Total Sent: %u bytes\n", simmodule.getTotalBytesSent());
    Serial.printf("[GSM] Total Received: %u bytes\n", simmodule.getTotalBytesReceived());
    uint32_t total = simmodule.getTotalBytesSent() + simmodule.getTotalBytesReceived();
    Serial.printf("[GSM] Total Traffic: %u bytes (%.2f KB)\n", total, total / 1024.0);
    Serial.printf("[GSM] Total Cycles: %u\n", simmodule.getCycleCount());
    if (simmodule.getCycleCount() > 0) {
        Serial.printf("[GSM] Avg per Cycle: %.2f KB\n", (total / 1024.0) / simmodule.getCycleCount());
    }
    Serial.println("[GSM] ========================\n");

    // Log to SD
    dataLogger.logGSMStats(rtc1.getDateTime().c_str(), 
                           simmodule.getTotalBytesSent(), 
                           simmodule.getTotalBytesReceived(), 
                           simmodule.getCycleCount());

    simmodule.disconnectGPRS();
    gsmPowerOff();
    Serial.println("[DC] TX Phase complete.");
}

WebServer configServer(80);

void handleRoot() {
    Preferences preferences;
    preferences.begin("weather_station", true);
    uint32_t currentInterval = preferences.getUInt("interval", 10);
    preferences.end();

    String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Weather Station Configuration</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --glass-bg: rgba(255, 255, 255, 0.03);
            --glass-border: rgba(255, 255, 255, 0.08);
            --accent: linear-gradient(135deg, #6366f1 0%, #a855f7 100%);
            --accent-hover: linear-gradient(135deg, #4f46e5 0%, #9333ea 100%);
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Outfit', sans-serif;
            background: var(--bg-grad);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
            overflow: hidden;
            position: relative;
        }
        body::before {
            content: '';
            position: absolute;
            width: 300px;
            height: 300px;
            background: rgba(99, 102, 241, 0.15);
            border-radius: 50%;
            top: -50px;
            left: -50px;
            filter: blur(80px);
            z-index: 0;
        }
        body::after {
            content: '';
            position: absolute;
            width: 300px;
            height: 300px;
            background: rgba(168, 85, 247, 0.15);
            border-radius: 50%;
            bottom: -50px;
            right: -50px;
            filter: blur(80px);
            z-index: 0;
        }
        .container {
            background: var(--glass-bg);
            border: 1px solid var(--glass-border);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border-radius: 24px;
            padding: 40px;
            width: 100%;
            max-width: 440px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            z-index: 10;
            text-align: center;
            animation: fadeIn 0.8s ease-out;
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }
        .logo {
            font-size: 2.5rem;
            font-weight: 600;
            background: var(--accent);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 8px;
            letter-spacing: -0.5px;
        }
        .subtitle {
            color: var(--text-muted);
            font-size: 0.95rem;
            margin-bottom: 32px;
        }
        .form-group {
            margin-bottom: 24px;
            text-align: left;
        }
        label {
            display: block;
            font-size: 0.875rem;
            font-weight: 600;
            margin-bottom: 8px;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .input-wrapper {
            position: relative;
            display: flex;
            align-items: center;
        }
        input[type="number"] {
            width: 100%;
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--glass-border);
            border-radius: 12px;
            padding: 14px 16px;
            font-family: inherit;
            font-size: 1rem;
            color: var(--text-main);
            outline: none;
            transition: all 0.3s;
        }
        input[type="number"]:focus {
            border-color: #6366f1;
            background: rgba(255, 255, 255, 0.08);
            box-shadow: 0 0 0 4px rgba(99, 102, 241, 0.15);
        }
        .unit {
            position: absolute;
            right: 16px;
            color: var(--text-muted);
            font-size: 0.9rem;
            pointer-events: none;
        }
        button {
            width: 100%;
            background: var(--accent);
            color: white;
            border: none;
            border-radius: 12px;
            padding: 16px;
            font-family: inherit;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            box-shadow: 0 10px 15px -3px rgba(99, 102, 241, 0.3);
            margin-top: 8px;
        }
        button:hover {
            background: var(--accent-hover);
            transform: translateY(-2px);
            box-shadow: 0 12px 20px -3px rgba(99, 102, 241, 0.4);
        }
        button:active {
            transform: translateY(0);
        }
        .footer {
            margin-top: 32px;
            font-size: 0.75rem;
            color: var(--text-muted);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">AURA</div>
        <div class="subtitle">Weather Station Control Center</div>
        <form action="/save" method="GET">
            <div class="form-group">
                <label for="interval">Sleeping Interval</label>
                <div class="input-wrapper">
                    <input type="number" id="interval" name="interval" min="1" max="1440" value="%CURRENT_INTERVAL%" required>
                    <span class="unit">min</span>
                </div>
            </div>
            <button type="submit">Apply & Reboot</button>
        </form>
        <div class="footer">Connected to Local AP • Changes persist across power cycles</div>
    </div>
</body>
</html>)rawliteral";

    html.replace("%CURRENT_INTERVAL%", String(currentInterval));
    configServer.send(200, "text/html", html);
}

void handleSave() {
    if (configServer.hasArg("interval")) {
        uint32_t newInterval = configServer.arg("interval").toInt();
        if (newInterval > 0 && newInterval <= 1440) {
            Preferences preferences;
            preferences.begin("weather_station", false);
            preferences.putUInt("interval", newInterval);
            preferences.end();

            String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Updating Station...</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --glass-bg: rgba(255, 255, 255, 0.03);
            --glass-border: rgba(255, 255, 255, 0.08);
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        body {
            font-family: 'Outfit', sans-serif;
            background: var(--bg-grad);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: var(--glass-bg);
            border: 1px solid var(--glass-border);
            backdrop-filter: blur(20px);
            border-radius: 24px;
            padding: 40px;
            width: 100%;
            max-width: 400px;
            text-align: center;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
        }
        .spinner {
            width: 50px;
            height: 50px;
            border: 3px solid rgba(255,255,255,0.1);
            border-radius: 50%;
            border-top-color: #6366f1;
            animation: spin 1s ease-in-out infinite;
            margin: 0 auto 24px auto;
        }
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        h2 { font-weight: 600; margin-bottom: 12px; }
        p { color: var(--text-muted); font-size: 0.95rem; }
    </style>
</head>
<body>
    <div class="container">
        <div class="spinner"></div>
        <h2>Applying Settings</h2>
        <p>Interval updated to %NEW_INTERVAL% minutes. The station is rebooting now...</p>
    </div>
</body>
</html>)rawliteral";

            html.replace("%NEW_INTERVAL%", String(newInterval));
            configServer.send(200, "text/html", html);
            delay(2000);
            ESP.restart();
            return;
        }
    }
    configServer.send(400, "text/plain", "Bad Request");
}

void runConfigPortal(unsigned long tonStart, bool isManualBoot) {
    Serial.println("[WiFi AP] Initializing WiFi Access Point...");
    WiFi.mode(WIFI_AP);
    
    if (WiFi.softAP("ESP32_Weather_Config")) {
        Serial.println("[WiFi AP] AP Started successfully!");
        Serial.print("[WiFi AP] IP Address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("[WiFi AP] AP Failed to start.");
        return;
    }

    configServer.on("/", handleRoot);
    configServer.on("/save", handleSave);
    configServer.begin();
    Serial.println("[WiFi AP] HTTP server started on port 80");

    unsigned long apStart = millis();
    // 1.5 minutes (90s) for manual boot/reset, 15s for scheduled timer wakeups
    const unsigned long AP_WAIT_TIMEOUT = isManualBoot ? 90000 : 15000;
    // 10 minutes max configuration window for manual boot, normal TON_MS for scheduled wakes
    unsigned long activeWindowLimit = isManualBoot ? (10ULL * 60ULL * 1000ULL) : TON_MS;

    Serial.printf("[WiFi AP] Config Portal ready. Timeout: %lu ms. Max active window: %lu ms.\n", 
                  AP_WAIT_TIMEOUT, activeWindowLimit);

    while (millis() - tonStart < activeWindowLimit) {
        configServer.handleClient();

        int numStations = WiFi.softAPgetStationNum();
        if (numStations > 0) {
            apStart = millis(); // Reset timeout as long as client is connected
        } else {
            if (millis() - apStart > AP_WAIT_TIMEOUT) {
                Serial.println("[WiFi AP] No clients connected within timeout. Shutting down portal.");
                break;
            }
        }
        delay(10);
    }

    configServer.close();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi AP] Portal shut down.");
}


void setup() {
    Serial.begin(115200);
    delay(200);
    
    // Load config from preferences
    Preferences preferences;
    preferences.begin("weather_station", true);
    updateIntervalMinutes = preferences.getUInt("interval", 10);
    preferences.end();
    
    Wire.setTimeOut(50); // Set global I2C timeout to prevent hangs
    
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    esp_reset_reason_t reset_reason = esp_reset_reason();
    bool isManualBoot = (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER);

    Serial.println("\n[DC] ================================");
    Serial.print("[DC] Wakeup reason: ");
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("External signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("External signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP:      Serial.println("ULP program"); break;
        default:                        Serial.printf("Not caused by deep sleep (%d)\n", wakeup_reason); break;
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
    Serial.println("[DC] after LoRa setup");

    unsigned long tonStart = millis();
    Serial.println("[DC] before readAllSensors");
    SensorData currentData = readAllSensors();
    Serial.println("[DC] after readAllSensors");

    Serial.println("[DC] before logToSD");
    logToSD(currentData);
    Serial.println("[DC] after logToSD");

    Serial.println("[DC] before transmitData");
    transmitData(currentData, tonStart);
    Serial.println("[DC] after transmitData");

    // Run WiFi Configuration Portal
    runConfigPortal(tonStart, isManualBoot);

    Serial.printf("[DC] Active window closed. Elapsed: %lu ms\n", millis() - tonStart);
    
    sdCardRelease();

    Serial.println("[DC] Sleep phase: entering deep sleep");
    enterDeepSleep(tonStart);
}

void loop() {
    // deep sleep happens in setup(); nothing to do here
}

