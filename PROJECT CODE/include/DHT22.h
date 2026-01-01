#pragma once
#include <string>
#define DHTPIN 5     // Digital pin connected to the DHT sensor
using namespace std;

class DHTSensor {
public: 
//pass only the pin number where the data pin of DHT22 is connected
    DHTSensor();
    float readTemperature(); // read temperature in Celsius
    float readHumidity(); // read humidity in percentage
    void getsensor();  // show sensor type
private:
    int pin_;   
    float temperature_;  
    float humidity_;
    
};  