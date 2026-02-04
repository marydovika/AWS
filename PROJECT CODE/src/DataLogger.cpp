#include "DataLogger.h"

DataLogger::DataLogger(int csPin) {
    _csPin = csPin;
    _fileName = "/datalog.txt";
    _lastDataString = "";
}

void DataLogger::begin() {
    if (!SD.begin(_csPin)) {
        Serial.println("SD Card Mount Failed");
        return;
    }
    Serial.println("SD Card Initialized");
}

void DataLogger::logSensorData(String timestamp, SensorData data) {
    // 1. Construct the String with Identifiers
    // Format: "Time:..., Press:..., Temp:...,"
    String dataStr = "";
    dataStr += "Time:" + timestamp + ",";
    dataStr += "Press:" + String(data.airPressure, 2) + ",";
    dataStr += "Alt:" + String(data.altitude, 2) + ",";
    dataStr += "Temp:" + String(data.temperature, 2) + ",";
    dataStr += "Hum:" + String(data.humidity, 2) + ",";
    dataStr += "Light:" + String(data.lightLevel, 2) + ",";
    dataStr += "SoilM:" + String(data.soilMoisture, 2) + ",";
    dataStr += "Rain:" + String(data.rainCount) + ",";
    dataStr += "WSpd:" + String(data.windSpeed, 2) + ",";
    dataStr += "WDir:" + String(data.windDirection) + ",";
    // Power
    dataStr += "V33:" + String(data.volt_3v3, 2) + ",";
    dataStr += "V5:" + String(data.volt_5v, 2) + ",";
    dataStr += "VBatt:" + String(data.volt_batt, 2) + ",";
    dataStr += "VSol:" + String(data.volt_solar, 2) + ",";
    dataStr += "VDC:" + String(data.volt_dc, 2);

    // 2. Write to SD
    File file = SD.open(_fileName, FILE_APPEND);
    if (file) {
        file.println(dataStr);
        file.close();
        Serial.println("Data Logged to SD: " + dataStr);
        
        // 3. Update the internal cache for the GSM function
        _lastDataString = dataStr; 
    } else {
        Serial.println("Error writing to SD");
    }
}

// Helper to find "Label:Value," and return Value
String DataLogger::getValueFromLog(String logLine, String label) {
    String searchKey = label + ":";
    int startIndex = logLine.indexOf(searchKey);
    if (startIndex == -1) return "0"; // Not found
    
    startIndex += searchKey.length();
    int endIndex = logLine.indexOf(",", startIndex);
    if (endIndex == -1) endIndex = logLine.length(); // End of line case
    
    return logLine.substring(startIndex, endIndex);
}

void DataLogger::uploadLastDataToThingspeak(GSM &gsmModule) {
    if (_lastDataString == "") {
        Serial.println("No data to upload.");
        return;
    }

    Serial.println("Deconstructing Log String for Upload...");

    // CHANNEL 1: Weather (8 Fields max)
    // Field 1: Temp, 2: Hum, 3: Press, 4: Rain, 5: WindSpd, 6: WindDir, 7: Light, 8: SoilM
    String url1 = "http://api.thingspeak.com/update?api_key=" + API_KEY_1;
    url1 += "&field1=" + getValueFromLog(_lastDataString, "Temp");
    url1 += "&field2=" + getValueFromLog(_lastDataString, "Hum");
    url1 += "&field3=" + getValueFromLog(_lastDataString, "Press");
    url1 += "&field4=" + getValueFromLog(_lastDataString, "Rain");
    url1 += "&field5=" + getValueFromLog(_lastDataString, "WSpd");
    url1 += "&field6=" + getValueFromLog(_lastDataString, "WDir");
    url1 += "&field7=" + getValueFromLog(_lastDataString, "Light");
    url1 += "&field8=" + getValueFromLog(_lastDataString, "SoilM");

    gsmModule.sendThingSpeakRequest(url1);
    delay(2000); // Small pause between requests

    // CHANNEL 2: Power Rails A (Voltage 3.3, 5, Battery)
    String url2 = "http://api.thingspeak.com/update?api_key=" + API_KEY_2;
    url2 += "&field1=" + getValueFromLog(_lastDataString, "V33");
    url2 += "&field2=" + getValueFromLog(_lastDataString, "V5");
    url2 += "&field3=" + getValueFromLog(_lastDataString, "VBatt");
    
    gsmModule.sendThingSpeakRequest(url2);
    delay(2000);

    // CHANNEL 3: Power Rails B (Solar, DC In, Altitude)
    String url3 = "http://api.thingspeak.com/update?api_key=" + API_KEY_3;
    url3 += "&field1=" + getValueFromLog(_lastDataString, "VSol");
    url3 += "&field2=" + getValueFromLog(_lastDataString, "VDC");
    url3 += "&field3=" + getValueFromLog(_lastDataString, "Alt");

    gsmModule.sendThingSpeakRequest(url3);
    Serial.println("All channels updated.");
}