#include <Arduino.h>
#include <Wire.h>

#define SLAVE_ADDR 0x08
#define PAYLOAD_SIZE 20 // 5 floats * 4 bytes

// Must match the struct structure in the STM32 code exactly
struct VoltageData {
    float v1;
    float v2;
    float v3;
    float v4;
    float v5;
};

VoltageData receivedData;

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C Master
  // SDA = GPIO 21, SCL = GPIO 22 (Default on ESP32 DevKit)
  Wire.begin(); 
  
  Serial.println("ESP32 I2C Master Initialized");
}

void loop() {
  Serial.print("Requesting data... ");

  // 1. Request 20 bytes from Slave
  uint8_t bytesReceived = Wire.requestFrom(SLAVE_ADDR, PAYLOAD_SIZE);

  if (bytesReceived == PAYLOAD_SIZE) {
    // 2. Read bytes directly into the struct memory
    Wire.readBytes((uint8_t*)&receivedData, PAYLOAD_SIZE);

    Serial.println("Success!");
    Serial.println("-------------------------");
    Serial.printf("3.3V Rail: %.2f V\n", receivedData.v1);
    Serial.printf("5V Rail:   %.2f V\n", receivedData.v2);
    Serial.printf("Battery:   %.2f V\n", receivedData.v3);
    Serial.printf("Solar:     %.2f V\n", receivedData.v4);
    Serial.printf("DC In:     %.2f V\n", receivedData.v5);
    Serial.println("-------------------------");
    
  } else {
    Serial.println("Failed.");
    Serial.print("Expected 20 bytes, got: ");
    Serial.println(bytesReceived);
    
    // Clear buffer if partial garbage arrived
    while(Wire.available()) Wire.read();
  }

  delay(1000);
}