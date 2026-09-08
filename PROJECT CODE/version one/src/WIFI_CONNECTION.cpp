#include "WIFI_CONNECTION.h"
#include <WiFi.h>
#include <Arduino.h>
using namespace std;

WIFI_CONNECTION::WIFI_CONNECTION(const string& ssid, const string& password) : ssid_(ssid), password_(password), connected_(false) {}

bool WIFI_CONNECTION::connect() {
    Serial.begin(9600);
    WiFi.begin(ssid_.c_str(), password_.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
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
