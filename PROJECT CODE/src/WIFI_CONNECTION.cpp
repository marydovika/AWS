#include "WIFI_CONNECTION.h"
#include <WiFi.h>
#include <Arduino.h>
#include "ERROR_LOGGER.h"
using namespace std;

WIFI_CONNECTION::WIFI_CONNECTION(const string& ssid, const string& password) : ssid_(ssid), password_(password), connected_(false) {}

bool WIFI_CONNECTION::connect() {
    Serial.begin(9600);
    WiFi.begin(ssid_.c_str(), password_.c_str());

    unsigned long secondsWaited = 0;
    bool timeoutLogged = false;

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
        secondsWaited++;

        // Log once after 30 seconds of failed attempts, but keep
        // retrying since the original firmware behaviour is to
        // block until connected (intentionally not changed here).
        if (secondsWaited == 30 && !timeoutLogged) {
            ErrorLogger::log(COMP_WIFI, ERR_WIFI_TIMEOUT,
                             "No connection after 30s - still retrying (blocking)");
            timeoutLogged = true;
        }
    }
    connected_ = true;
    return connected_;
}

void WIFI_CONNECTION::disconnect() {
    Serial.begin(9600);
    WiFi.disconnect();
    Serial.println("Disconnected from WiFi.");
    connected_ = false;
}

bool WIFI_CONNECTION::isConnected() const {
    return connected_;
}

std::string WIFI_CONNECTION::getSSID() const {
    return ssid_;
}
