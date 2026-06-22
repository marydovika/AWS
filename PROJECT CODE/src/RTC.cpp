#include "RTC.h"
#include "ERROR_LOGGER.h"
#include <Wire.h>
#include <WiFi.h>
#include <time.h>

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

Rtc::Rtc() : date_time_(""), rtcFound(false) {}

void Rtc::setupRTC() {
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        rtcFound = false;
        ErrorLogger::log(COMP_RTC, ERR_RTC_NOT_FOUND, "DS3231 not responding on I2C");
    } else {
        rtcFound = true;
    }

    if (rtcFound && rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        ErrorLogger::log(COMP_RTC, ERR_RTC_POWER_LOSS, "Battery depleted - reset to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

bool Rtc::syncWithNTP(const char* ntpServer, long gmtOffsetSec, int daylightOffsetSec) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot sync NTP");
        return false;
    }

    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

    struct tm timeinfo;
    Serial.print("Waiting for NTP time sync...");

    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        Serial.print(".");
        delay(500);
        retry++;
    }

    if (retry >= 10) {
        Serial.println("\nFailed to get NTP time");
        ErrorLogger::log(COMP_RTC, ERR_RTC_NTP_SYNC_FAIL, "10 retries exhausted");
        return false;
    }

    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));

    Serial.println("\nRTC synchronized with NTP successfully.");
    return true;
}

bool Rtc::syncWithGSM(String gsmTime) {
    if (!rtcFound) return false; // root cause already logged in setupRTC()

    if (gsmTime.length() < 17) {
        Serial.println("[RTC] Invalid GSM time format.");
        ErrorLogger::log(COMP_RTC, ERR_RTC_GSM_FORMAT_INVALID, gsmTime.c_str());
        return false;
    }

    int year  = gsmTime.substring(0, 2).toInt() + 2000;
    int month = gsmTime.substring(3, 5).toInt();
    int day   = gsmTime.substring(6, 8).toInt();
    int hour  = gsmTime.substring(9, 11).toInt();
    int min   = gsmTime.substring(12, 14).toInt();
    int sec   = gsmTime.substring(15, 17).toInt();

    if (year < 2024) {
        Serial.println("[RTC] GSM time seems invalid (pre-2024). Skipping sync.");
        ErrorLogger::log(COMP_RTC, ERR_RTC_GSM_TIME_IMPLAUSIBLE,
                         ("Year reported: " + String(year)).c_str());
        return false;
    }

    DateTime gsmDt(year, month, day, hour, min, sec);
    TimeSpan eatOffset(0, 3, 0, 0);
    DateTime localDt = gsmDt + eatOffset;

    // ── Clock drift check ─────────────────────────────────
    // Compare what the RTC currently believes against the
    // GSM network time (trusted reference) BEFORE overwriting it.
    // This is what catches a clock that is running but wrong,
    // as opposed to a clock that has simply lost power.
    DateTime before = rtc.now();
    long driftSeconds = (long)localDt.unixtime() - (long)before.unixtime();
    if (abs(driftSeconds) > 300) { // more than 5 minutes off
        String detail = "RTC was " + String(driftSeconds) + "s off before re-sync";
        ErrorLogger::log(COMP_RTC, ERR_RTC_DRIFT_DETECTED, detail.c_str());
    }

    rtc.adjust(localDt);
    Serial.println("[RTC] Synchronized with GSM network time (EAT Offset applied).");
    return true;
}

void Rtc::printDateTime() {
    std::string currentTime = getDateTime();
    Serial.println(currentTime.c_str());
}

std::string Rtc::getDateTime() {
    if (!rtcFound) {
        return "2000-01-01 00:00:00";
    }
    DateTime now = rtc.now();

    char buf[100];
    snprintf(buf, sizeof(buf), "%s, %04d-%02d-%02d %02d:%02d:%02d",
             daysOfTheWeek[now.dayOfTheWeek()],
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    date_time_ = std::string(buf);
    return date_time_;
}