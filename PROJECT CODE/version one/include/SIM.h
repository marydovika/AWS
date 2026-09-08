#pragma once
#ifndef SIM_H
#define SIM_H
#include <string>
using namespace std;

class Sim {
public:
    Sim();
    void setupSIM();
    bool connectNetwork(const string& apn, const string& gprsUser = "", const string& gprsPass = "");
    bool isConnected(); 
    void sendData(const string& server_url_path, const string& data);

private:
    bool connected_;
    string apn_;
    string data_;
    string server_url_path_;
    string gprsUser_;
    string gprsPass_;
};

#endif
