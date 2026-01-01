#pragma once
#include <string>
using namespace std;

class WIFI_CONNECTION {
public:
    WIFI_CONNECTION(const string& ssid, const string& password);
    bool connect();
    void disconnect();
    bool isConnected() const;
    string getSSID() const;
private:
    string ssid_;  
    string password_;
    bool connected_;
};
