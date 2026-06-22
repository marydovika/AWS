#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <RTClib.h>
#include <string>

class Rtc {
public:
    Rtc();
    void setupRTC();
    bool syncWithNTP(const char* ntpServer, long gmtOffsetSec, int daylightOffsetSec);
    bool syncWithGSM(String gsmTime);
    std::string getDateTime();
    void printDateTime();
    RTC_DS3231* getRTCHandle() { return rtcFound ? &rtc : nullptr; }

private:
    RTC_DS3231 rtc;
    std::string date_time_;
    bool rtcFound;
};

#endif