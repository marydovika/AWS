#include "LORA.h"
#include "ERROR_LOGGER.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#define RX 14
#define TX 15
HardwareSerial SerialL = Serial1;

Lora::Lora() {}

void Lora::setupLora() {
    SerialL.begin(9600, SERIAL_8N1, RX, TX);
    if(SerialL) {
        Serial.println("LoRa module serial initialized.");
    } else {
        Serial.println("Failed to initialize LoRa module serial.");
        ErrorLogger::log(COMP_LORA, ERR_LORA_SERIAL_INIT_FAIL, "SerialL object invalid after begin()");
    }
}

void Lora::sendData(const String& command, int timeout) {
    Serial.print("Sending: ");
    Serial.println(command);
  
    while(SerialL.available()) SerialL.read();
  
    SerialL.println(command);

    bool receivedAnyResponse = false;
    unsigned long start = millis();
    while (millis() - start < (unsigned long)timeout) {
        while (SerialL.available()) {
            char c = SerialL.read();
            Serial.write(c);
            receivedAnyResponse = true;
        }
        delay(1);
    }
    Serial.println("\n-----------------------");

    if (!receivedAnyResponse) {
        ErrorLogger::log(COMP_LORA, ERR_LORA_NO_RESPONSE, ("No reply to: " + command).c_str());
    }
}